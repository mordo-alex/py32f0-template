#include "gun_logic.h"
#include "adc_core.h"
#include "py32f0xx_bsp_printf.h"
#include <stdlib.h>

#if GUN_DEBUG_EN
    #define GUN_LOG(fmt, ...) printf("[Gun] " fmt, ##__VA_ARGS__)
#else
    #define GUN_LOG(fmt, ...) 
#endif

// ==========================================
// ★ 动作宏定义
// ==========================================
#define FAN_KILL()              HAL_GPIO_WritePin(GUN_FAN_PORT, GUN_FAN_PIN, GPIO_PIN_SET)
#define FAN_RUN()               HAL_GPIO_WritePin(GUN_FAN_PORT, GUN_FAN_PIN, GPIO_PIN_RESET)

#define HEATER_ON()             HAL_GPIO_WritePin(GUN_HEAT_PORT, GUN_HEAT_PIN, GPIO_PIN_RESET)
#define HEATER_OFF()            HAL_GPIO_WritePin(GUN_HEAT_PORT, GUN_HEAT_PIN, GPIO_PIN_SET)

#define IS_ON_CRADLE()          (HAL_GPIO_ReadPin(GUN_REED_PORT, GUN_REED_PIN) == GPIO_PIN_RESET)
#define IS_GUN_SWITCH_ON()      (HAL_GPIO_ReadPin(GUN_MASTER_SW_PORT, GUN_MASTER_SW_PIN) == GPIO_PIN_RESET)

GunPID_Config_t gun_pid = { .Kp = 2.0f, .Ki = 0.1f, .Kd = 1.0f };
static GunState_t current_state = GUN_STANDBY;
static GunState_t prev_state = GUN_STANDBY; 
static int current_real_temp = 25;

static uint16_t raw_adc_val = 0; 
static int tune_target = 0;
static int tune_cycles = 0;

void Gun_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = GUN_MASTER_SW_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GUN_MASTER_SW_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GUN_REED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GUN_REED_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GUN_FAN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GUN_FAN_PORT, &GPIO_InitStruct);
    FAN_KILL();

    GPIO_InitStruct.Pin = GUN_HEAT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GUN_HEAT_PORT, &GPIO_InitStruct);
    HEATER_OFF();

    GUN_LOG("Gun Hardware Init OK.\r\n");
}

static void Update_Real_Temp(void)
{
    raw_adc_val = ADC_Read_Gun_Lazy();
    
    // ★ 恢复开路检测！只要 ADC 飙到 2800 以上，说明测温脚悬空(拔出)
    if (raw_adc_val > 2800)
    {
        current_real_temp = 999; // 999 为开路标志
    }
    else
    {
        current_real_temp = (raw_adc_val * 500) / 2048; 
    }
}

static void Software_PWM_Drive(int target_power_percent)
{
    static uint8_t pwm_tick = 0;
    uint8_t threshold = (target_power_percent * 20) / 100;
    
    if ((pwm_tick < threshold) && (target_power_percent > 0))
    {
        HEATER_ON();
    }
    else
    {
        HEATER_OFF();
    }
    
    pwm_tick++;
    if (pwm_tick >= 20)
    {
        pwm_tick = 0;
    }
}

static int Gun_PID_Compute(int current, int target)
{
    static int error_prev = 0;
    static float integral = 0;
    int error = target - current;
    float P_out = gun_pid.Kp * error;
    float D_out = 0.0f;
    int output = 0;

    if (abs(error) < 50)
    {
        integral += error * gun_pid.Ki;
    }
    else
    {
        integral = 0;
    }

    if (integral > 100)
    {
        integral = 100;
    }
    if (integral < 0)
    {
        integral = 0;
    }

    D_out = gun_pid.Kd * (error - error_prev);
    error_prev = error;
    output = (int)(P_out + integral + D_out);

    if (output > 100)
    {
        output = 100;
    }
    if (output < 0)
    {
        output = 0;
    }

    return output;
}

static void Process_AutoTune(void)
{
    static uint8_t heater_state = 1;
    float Ku = 15.5f;
    float Tu = 10.0f;
    
    FAN_RUN();

    if (heater_state == 1)
    {
        Software_PWM_Drive(100);
        if (current_real_temp > tune_target)
        {
            heater_state = 0;
        }
    }
    else
    {
        Software_PWM_Drive(0);
        if (current_real_temp < tune_target)
        {
            heater_state = 1;
            tune_cycles++;
            GUN_LOG("Tune Cycle %d OK.\r\n", tune_cycles);
        }
    }

    if (tune_cycles >= 3)
    {
        gun_pid.Kp = 0.6f * Ku;
        gun_pid.Ki = (1.2f * Ku) / Tu;
        gun_pid.Kd = (0.075f * Ku) * Tu;
        
        GUN_LOG("Tune DONE! Kp:%.2f\r\n", gun_pid.Kp);
        current_state = GUN_RUNNING;
    }
}

void Gun_Start_AutoTune(int target_temp)
{
    if ((current_state == GUN_RUNNING) || (current_state == GUN_STANDBY))
    {
        tune_target = target_temp;
        tune_cycles = 0;
        current_state = GUN_AUTO_TUNING;
        GUN_LOG("Tune STARTED! Target: %d C\r\n", tune_target);
    }
}

void Gun_Process(int target_temp)
{
    Update_Real_Temp();

    // ==========================================
    // ★ 变态级硬件引脚追踪 Log
    // ==========================================
    static uint8_t last_sw = 0xFF;
    static uint8_t last_cradle = 0xFF;
    static uint8_t last_plug = 0xFF;
    
    uint8_t curr_sw = IS_GUN_SWITCH_ON();
    uint8_t curr_cradle = IS_ON_CRADLE();
    uint8_t curr_plug = (current_real_temp == 999) ? 0 : 1; // 0=拔出, 1=插上

    if (curr_sw != last_sw)
    {
        GUN_LOG(">>> EVENT: Main Switch [PA8] -> %s <<<\r\n", curr_sw ? "ON (闭合)" : "OFF (断开)");
        last_sw = curr_sw;
    }

    if (curr_cradle != last_cradle)
    {
        GUN_LOG(">>> EVENT: Reed Switch [PB4] -> %s <<<\r\n", curr_cradle ? "GROUNDED (在架子上)" : "HIGH (拿起来了)");
        last_cradle = curr_cradle;
    }

    if (curr_plug != last_plug)
    {
        GUN_LOG(">>> EVENT: Aviation Plug -> %s (Raw ADC: %d) <<<\r\n", curr_plug ? "INSERTED (已插上)" : "REMOVED (已拔出)", raw_adc_val);
        last_plug = curr_plug;
    }

    // ==========================================
    // ★ 核心状态机流转 (加入航插检测)
    // ==========================================
    if (!curr_sw)
    {
        current_state = GUN_OFF;
    }
    else if (current_state == GUN_OFF)
    {
        current_state = GUN_STANDBY;
    }
    
    // 如果开关闭合，处理航插异常状态
    if (curr_sw)
    {
        if (current_real_temp == 999 && current_state != GUN_ERROR)
        {
            // 一旦发现是 999(拔出)，无条件切入 ERROR
            current_state = GUN_ERROR;
        }
        else if (current_state == GUN_ERROR && current_real_temp != 999)
        {
            // 从 ERROR 恢复时，回到 STANDBY
            current_state = GUN_STANDBY;
        }
    }

    // 状态变更 Log
    if (current_state != prev_state)
    {
        switch(current_state)
        {
            case GUN_OFF:       GUN_LOG("=== STATE: -> OFF ===\r\n"); break;
            case GUN_STANDBY:   GUN_LOG("=== STATE: -> STANDBY ===\r\n"); break;
            case GUN_RUNNING:   GUN_LOG("=== STATE: -> RUNNING ===\r\n"); break;
            case GUN_COOLING:   GUN_LOG("=== STATE: -> COOLING ===\r\n"); break;
            case GUN_ERROR:     GUN_LOG("=== STATE: -> ERROR (No Plug) ===\r\n"); break;
            case GUN_AUTO_TUNING: GUN_LOG("=== STATE: -> AUTO TUNING ===\r\n"); break;
            default: break;
        }
        prev_state = current_state;
    }

    // 状态机执行
    switch (current_state)
    {
        case GUN_OFF:
            HEATER_OFF();
            FAN_KILL();
            break;

        case GUN_ERROR:
            // 拔出航插，强杀所有输出，确保绝对安全！
            HEATER_OFF();
            FAN_KILL();
            break;
            
        case GUN_STANDBY:
            HEATER_OFF();
            FAN_KILL();
            
            if (!curr_cradle)
            { 
                current_state = GUN_RUNNING; 
            }
            break;
            
        case GUN_RUNNING:
            FAN_RUN(); 
            
            if (curr_cradle)
            {
                HEATER_OFF();
                current_state = GUN_COOLING;
            }
            else
            {
                Software_PWM_Drive(Gun_PID_Compute(current_real_temp, target_temp));
            }
            break;
            
        case GUN_COOLING:
            HEATER_OFF();
            FAN_RUN(); 
            
            if (!curr_cradle)
            {
                current_state = GUN_RUNNING;
            }
            // 冷却完毕，正常关机
            else if (current_real_temp <= 100)
            {
                current_state = GUN_STANDBY;
            }
            break;
            
        case GUN_AUTO_TUNING:
            if (curr_cradle)
            {
                GUN_LOG("Tune ABORTED (Returned to cradle)!\r\n");
                current_state = GUN_COOLING;
            }
            else
            {
                Process_AutoTune();
            }
            break;
            
        default:
            break;
    }
}

int Gun_GetRealTemp(void) { return current_real_temp; }
GunState_t Gun_GetState(void) { return current_state; }
