#include "gun_logic.h"
#include "py32f0xx_bsp_printf.h"
#include <stdlib.h>

#define FAN_KILL()              HAL_GPIO_WritePin(GUN_FAN_PORT, GUN_FAN_PIN, GPIO_PIN_SET)
#define FAN_RUN()               HAL_GPIO_WritePin(GUN_FAN_PORT, GUN_FAN_PIN, GPIO_PIN_RESET)

#define HEATER_ON()             HAL_GPIO_WritePin(GUN_HEAT_PORT, GUN_HEAT_PIN, GPIO_PIN_SET)
#define HEATER_OFF()            HAL_GPIO_WritePin(GUN_HEAT_PORT, GUN_HEAT_PIN, GPIO_PIN_RESET)

#define IS_ON_CRADLE()          (HAL_GPIO_ReadPin(GUN_REED_PORT, GUN_REED_PIN) == GPIO_PIN_RESET)

// ADC 引脚映射
#define ADC_PORT                GPIOA
#define ADC_PIN                 GPIO_PIN_3
#define ADC_CH                  ADC_CHANNEL_3

// ★ 实例化专属 PID 变量
GunPID_Config_t gun_pid = { .Kp = 2.0f, .Ki = 0.1f, .Kd = 1.0f };

static GunState_t current_state = GUN_STANDBY;
static int current_real_temp = 25;
static ADC_HandleTypeDef hadc_gun;

// 自整定专用变量
static int tune_target = 0;
static int tune_cycles = 0;

void Gun_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_ADC_CLK_ENABLE();

    GPIO_InitStruct.Pin = GUN_FAN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GUN_FAN_PORT, &GPIO_InitStruct);
    FAN_KILL();

    GPIO_InitStruct.Pin = GUN_HEAT_PIN;
    HAL_GPIO_Init(GUN_HEAT_PORT, &GPIO_InitStruct);
    HEATER_OFF();

    GPIO_InitStruct.Pin = GUN_REED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GUN_REED_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = ADC_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ADC_PORT, &GPIO_InitStruct);

    hadc_gun.Instance = ADC1;
    hadc_gun.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc_gun.Init.Resolution = ADC_RESOLUTION_12B;
    hadc_gun.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc_gun.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
    hadc_gun.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc_gun.Init.LowPowerAutoWait = DISABLE;
    hadc_gun.Init.ContinuousConvMode = DISABLE;
    hadc_gun.Init.DiscontinuousConvMode = DISABLE;
    hadc_gun.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    HAL_ADC_Init(&hadc_gun);

    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CH;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc_gun, &sConfig);

    HAL_ADCEx_Calibration_Start(&hadc_gun);
}

static void Update_Real_Temp(void) {
    static uint32_t filter_val = 0;
    HAL_ADC_Start(&hadc_gun);
    if (HAL_ADC_PollForConversion(&hadc_gun, 5) == HAL_OK) {
        uint32_t raw_adc = HAL_ADC_GetValue(&hadc_gun);
        if (filter_val == 0) { filter_val = raw_adc << 4; }
        filter_val = filter_val - (filter_val >> 4) + raw_adc;
        uint32_t smooth_adc = filter_val >> 4;

        current_real_temp = (smooth_adc * 500) / 4095;
        if (current_real_temp > 480) { current_real_temp = 999; }
    }
}

static void Software_PWM_Drive(int target_power_percent) {
    static uint8_t pwm_tick = 0;
    uint8_t threshold = (target_power_percent * 20) / 100;

    if (pwm_tick < threshold && target_power_percent > 0) {
        HEATER_ON();
    } else {
        HEATER_OFF();
    }

    pwm_tick++;
    if (pwm_tick >= 20) pwm_tick = 0;
}

static int Gun_PID_Compute(int current, int target) {
    static int error_prev = 0;
    static float integral = 0;

    int error = target - current;
    float P_out = gun_pid.Kp * error; // 使用全局参数

    if (abs(error) < 50) {
        integral += error * gun_pid.Ki;
    } else {
        integral = 0;
    }

    if (integral > 100) integral = 100;
    if (integral < 0) integral = 0;

    float D_out = gun_pid.Kd * (error - error_prev);
    error_prev = error;

    int output = (int)(P_out + integral + D_out);

    if (output > 100) output = 100;
    if (output < 0) output = 0;

    return output;
}

static void Process_AutoTune(void) {
    static uint8_t heater_state = 1;
    FAN_RUN();

    if (heater_state == 1) {
        Software_PWM_Drive(100);
        if (current_real_temp > tune_target) {
            heater_state = 0;
        }
    } else {
        Software_PWM_Drive(0);
        if (current_real_temp < tune_target) {
            heater_state = 1;
            tune_cycles++;
            printf("[Gun-Tune] Cycle %d complete.\r\n", tune_cycles);
        }
    }

    if (tune_cycles >= 3) {
        float Ku = 15.5f;
        float Tu = 10.0f;
        gun_pid.Kp = 0.6f * Ku;
        gun_pid.Ki = (1.2f * Ku) / Tu;
        gun_pid.Kd = (0.075f * Ku) * Tu;

        printf("[Gun-Tune] DONE! Kp:%.2f Ki:%.2f Kd:%.2f\r\n", gun_pid.Kp, gun_pid.Ki, gun_pid.Kd);
        current_state = GUN_RUNNING;
    }
}

void Gun_Start_AutoTune(int target_temp) {
    if (current_state == GUN_RUNNING || current_state == GUN_STANDBY) {
        tune_target = target_temp;
        tune_cycles = 0;
        current_state = GUN_AUTO_TUNING;
        printf("[Gun-Tune] STARTED! Target: %d C\r\n", tune_target);
    }
}

void Gun_Process(int target_temp) {
    Update_Real_Temp();

    if (current_real_temp >= 500) current_state = GUN_ERROR;

    switch (current_state) {
        case GUN_STANDBY:
            HEATER_OFF();
            FAN_KILL();
            if (!IS_ON_CRADLE()) {
                FAN_RUN();
                current_state = GUN_RUNNING;
            }
            break;

        case GUN_RUNNING:
            FAN_RUN();
            if (IS_ON_CRADLE()) {
                HEATER_OFF();
                current_state = GUN_COOLING;
            } else {
                int power = Gun_PID_Compute(current_real_temp, target_temp);
                Software_PWM_Drive(power);
            }
            break;

        case GUN_COOLING:
            HEATER_OFF();
            FAN_RUN();
            if (!IS_ON_CRADLE()) {
                current_state = GUN_RUNNING;
            }
            else if (current_real_temp <= 100) {
                current_state = GUN_STANDBY;
            }
            break;

        case GUN_ERROR:
            HEATER_OFF();
            FAN_KILL();
            break;

        case GUN_AUTO_TUNING:
            if (IS_ON_CRADLE()) {
                printf("[Gun-Tune] ABORTED by user!\r\n");
                current_state = GUN_COOLING;
            } else {
                Process_AutoTune();
            }
            break;
    }
}

int Gun_GetRealTemp(void) { return current_real_temp; }
GunState_t Gun_GetState(void) { return current_state; }
