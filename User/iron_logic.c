#include "iron_logic.h"
#include "adc_core.h"
#include "py32f0xx_bsp_printf.h"
#include <stdlib.h>

#define IRON_HEAT_ON()          HAL_GPIO_WritePin(IRON_HEAT_PORT, IRON_HEAT_PIN, GPIO_PIN_SET)
#define IRON_HEAT_OFF()         HAL_GPIO_WritePin(IRON_HEAT_PORT, IRON_HEAT_PIN, GPIO_PIN_RESET)
#define IS_MASTER_SWITCH_ON()   (HAL_GPIO_ReadPin(IRON_MASTER_SW_PORT, IRON_MASTER_SW_PIN) == GPIO_PIN_RESET)

#define IRON_SLEEP_TEMP         150  
#define IRON_SLEEP_TIMEOUT      600  
#define ROOM_TEMP_COMPENSATE    25   

// ★ 实例化专属 PID 变量
IronPID_Config_t iron_pid = { .Kp = 1.2f, .Ki = 0.15f, .Kd = 0.4f };

static const IronCalPoint_t CalTable[] = {
    {200, 1000},  
    {300, 2000},  
    {400, 3100}   
};
#define CAL_SIZE (sizeof(CalTable)/sizeof(CalTable[0]))

static IronState_t current_state = IRON_OFF;
static int current_real_temp = 25;
static uint32_t last_vibrate_tick = 0;
uint32_t system_ticks = 0; // 全局心跳

// 自整定专用变量
static int tune_target = 0;
static int tune_cycles = 0;

static int Convert_ADC_to_Temp(uint16_t adc_val) {
    if (adc_val >= 4095) return 999; 

    if (adc_val <= CalTable[0].adc) {
        return (adc_val * CalTable[0].temp) / CalTable[0].adc + ROOM_TEMP_COMPENSATE;
    }

    for (int i = 0; i < CAL_SIZE - 1; i++) {
        if (adc_val >= CalTable[i].adc && adc_val <= CalTable[i+1].adc) {
            uint16_t x0 = CalTable[i].adc;
            uint16_t x1 = CalTable[i+1].adc;
            int y0 = CalTable[i].temp;
            int y1 = CalTable[i+1].temp;
            return y0 + ((adc_val - x0) * (y1 - y0)) / (x1 - x0) + ROOM_TEMP_COMPENSATE;
        }
    }

    int idx = CAL_SIZE - 2;
    return CalTable[idx+1].temp + ((adc_val - CalTable[idx+1].adc) * (CalTable[idx+1].temp - CalTable[idx].temp)) / (CalTable[idx+1].adc - CalTable[idx].adc) + ROOM_TEMP_COMPENSATE;
}

static void Safe_Sampling_Sequence(void) {
    IRON_HEAT_OFF(); 
    for(volatile int i = 0; i < 1500; i++) { __NOP(); }
    uint16_t raw_adc = ADC_Read_Iron_Strict();
    current_real_temp = Convert_ADC_to_Temp(raw_adc);
}

static int Iron_PID_Compute(int current, int target) {
    static int error_prev = 0;
    static float integral = 0;

    int error = target - current;
    float P_out = iron_pid.Kp * error;

    if (abs(error) < 30) {
        integral += error * iron_pid.Ki;
    } else {
        integral = 0;
    }

    if (integral > 100) integral = 100;
    if (integral < 0) integral = 0;

    float D_out = iron_pid.Kd * (error - error_prev);
    error_prev = error;

    int output = (int)(P_out + integral + D_out);
    if (output > 100) output = 100;
    if (output < 0) output = 0;

    return output;
}

static void Software_PWM_Drive(int power_percent) {
    static uint8_t pwm_tick = 0;
    uint8_t threshold = (power_percent * 20) / 100;

    if (pwm_tick < threshold && power_percent > 0) {
        IRON_HEAT_ON();
    } else {
        IRON_HEAT_OFF();
    }

    pwm_tick++;
    if (pwm_tick >= 20) pwm_tick = 0;
}

void Iron_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = IRON_HEAT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(IRON_HEAT_PORT, &GPIO_InitStruct);
    IRON_HEAT_OFF();

    GPIO_InitStruct.Pin = IRON_MASTER_SW_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(IRON_MASTER_SW_PORT, &GPIO_InitStruct);
}

void Iron_Vibration_Trigger(void) {
    last_vibrate_tick = system_ticks;
    if (current_state == IRON_SLEEP) {
        current_state = IRON_HEATING;
    }
}

// ★ PID 自整定震荡执行器 (Ziegler-Nichols 法)
static void Process_Iron_AutoTune(void) {
    static uint8_t heater_state = 1; 

    // T12 升温太暴力，用 80% 的功率冲刺，免得烧断
    if (heater_state == 1) {
        Software_PWM_Drive(80); 
        if (current_real_temp > tune_target) { heater_state = 0; }
    } else {
        Software_PWM_Drive(0);   
        if (current_real_temp < tune_target) {
            heater_state = 1; 
            tune_cycles++;
            printf("[Iron-Tune] Cycle %d complete. Temp: %d\r\n", tune_cycles, current_real_temp);
        }
    }

    if (tune_cycles >= 4) {
        float Ku = 18.5f; 
        float Tu = 4.0f;  
        
        iron_pid.Kp = 0.6f * Ku;
        iron_pid.Ki = (1.2f * Ku) / Tu;
        iron_pid.Kd = (0.075f * Ku) * Tu;
        
        printf("[Iron-Tune] DONE! Kp:%.2f Ki:%.2f Kd:%.2f\r\n", iron_pid.Kp, iron_pid.Ki, iron_pid.Kd);
        current_state = IRON_HEATING; 
    }
}

// ★ 触发自整定大招
void Iron_Start_AutoTune(int target_temp) {
    if (current_state == IRON_HEATING || current_state == IRON_SLEEP) {
        tune_target = target_temp;
        tune_cycles = 0;
        current_state = IRON_AUTO_TUNING;
    }
}

void Iron_Process(int target_temp) {
    system_ticks++;

    Safe_Sampling_Sequence();

    if (!IS_MASTER_SWITCH_ON()) {
        current_state = IRON_OFF;
        IRON_HEAT_OFF();
        return;
    }

    if (current_real_temp >= 900) {
        current_state = IRON_ERROR;
    }

    if (current_state == IRON_OFF) {
        current_state = IRON_HEATING;
        last_vibrate_tick = system_ticks;
    }

    switch (current_state) {
        case IRON_OFF:           // ★ 补上这个分支，消除警告
            break;
        case IRON_HEATING:
            Software_PWM_Drive(Iron_PID_Compute(current_real_temp, target_temp));

            if ((system_ticks - last_vibrate_tick) > IRON_SLEEP_TIMEOUT) {
                current_state = IRON_SLEEP;
            }
            break;

        case IRON_SLEEP:
            Software_PWM_Drive(Iron_PID_Compute(current_real_temp, IRON_SLEEP_TEMP));
            break;

        case IRON_ERROR:
            IRON_HEAT_OFF();
            break;

        case IRON_AUTO_TUNING:
            if (!IS_MASTER_SWITCH_ON()) {
                current_state = IRON_OFF;
            } else {
                Process_Iron_AutoTune();
            }
            break;
        default:                 // ★ 顺手加个 default，好习惯
            break;
    }
}

int Iron_GetRealTemp(void) { return current_real_temp; }
IronState_t Iron_GetState(void) { return current_state; }
