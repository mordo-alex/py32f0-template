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
// ★ 底层动作宏定义
// ==========================================
#define FAN_KILL()              HAL_GPIO_WritePin(GUN_FAN_PORT, GUN_FAN_PIN, GPIO_PIN_SET)
#define FAN_RUN()               HAL_GPIO_WritePin(GUN_FAN_PORT, GUN_FAN_PIN, GPIO_PIN_RESET)

// 0V (RESET) 灌入电流导通光耦 -> 开始加热
#define HEATER_ON()             HAL_GPIO_WritePin(GUN_HEAT_PORT, GUN_HEAT_PIN, GPIO_PIN_RESET)
// 5V (SET) 消除电位差关闭光耦 -> 停止加热
#define HEATER_OFF()            HAL_GPIO_WritePin(GUN_HEAT_PORT, GUN_HEAT_PIN, GPIO_PIN_SET)

#define IS_ON_CRADLE()          (HAL_GPIO_ReadPin(GUN_REED_PORT, GUN_REED_PIN) == GPIO_PIN_RESET)
#define IS_GUN_SWITCH_ON()      (HAL_GPIO_ReadPin(GUN_MASTER_SW_PORT, GUN_MASTER_SW_PIN) == GPIO_PIN_RESET)

GunPID_Config_t gun_pid = { .Kp = 2.0f, .Ki = 0.1f, .Kd = 1.0f };
static GunState_t current_state = GUN_STANDBY;
static GunState_t prev_state = GUN_STANDBY; 
static int current_real_temp = 25;
static int tune_target = 0;
static int tune_cycles = 0;

// ★ 新增：风枪安全武装锁 (0=未武装，1=已武装允许加热)
static uint8_t gun_armed = 0; 

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

    gun_armed = 0; // 开机默认未武装
    GUN_LOG("Init OK. Unarmed.\r\n");
}

static void Update_Real_Temp(void)
{
    uint16_t raw_adc = ADC_Read_Gun_Lazy();
    current_real_temp = (raw_adc * 500) / 4095; 
    
    if (current_real_temp > 480)
    {
        current_real_temp = 999;
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

    if (!IS_GUN_SWITCH_ON())
    {
        current_state = GUN_OFF;
        HEATER_OFF();
        
        if (current_real_temp > 100)
        {
            FAN_RUN();
        }
        else
        {
            FAN_KILL();
        }
        
        gun_armed = 0; // 断开开关，立刻剥夺武装
        return; 
    }

    if (current_state == GUN_OFF)
    {
        current_state = GUN_STANDBY;
        gun_armed = 0; // 刚合上开关，强制要求先放回架子
        GUN_LOG("Switch ON, Wait for Cradle.\r\n");
    }

    // 报错复活逻辑
    if (current_real_temp >= 500)
    {
        current_state = GUN_ERROR;
    }
    else if ((current_state == GUN_ERROR) && (current_real_temp < 500))
    {
        current_state = GUN_STANDBY; 
        GUN_LOG("Sensor restored.\r\n");
    }

    if (current_state != prev_state)
    {
        switch(current_state)
        {
            case GUN_OFF:
                GUN_LOG("State -> OFF\r\n");
                break;
            case GUN_STANDBY:
                GUN_LOG("State -> STANDBY\r\n");
                break;
            case GUN_RUNNING:
                GUN_LOG("State -> RUNNING\r\n");
                break;
            case GUN_COOLING:
                GUN_LOG("State -> COOLING\r\n");
                break;
            case GUN_ERROR:
                GUN_LOG("State -> ERROR!\r\n");
                break;
            default:
                break;
        }
        prev_state = current_state;
    }

    switch (current_state)
    {
        case GUN_OFF:
            break;
            
        case GUN_STANDBY:
            HEATER_OFF();
            FAN_KILL();
            
            // 核心安全补丁：必须检测到一次放回架子，才允许解锁武装
            if (IS_ON_CRADLE())
            {
                gun_armed = 1; 
            }
            
            // 只有处于武装状态，且被拿起来，才进入运行
            if (!IS_ON_CRADLE() && gun_armed)
            { 
                FAN_RUN(); 
                current_state = GUN_RUNNING; 
            }
            break;
            
        case GUN_RUNNING:
            FAN_RUN();
            if (IS_ON_CRADLE())
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
            if (!IS_ON_CRADLE())
            {
                current_state = GUN_RUNNING;
            }
            else if (current_real_temp <= 100)
            {
                current_state = GUN_STANDBY;
            }
            break;
            
        case GUN_ERROR: 
            HEATER_OFF(); 
            FAN_KILL(); 
            gun_armed = 0; // 报错时自动剥夺武装
            break;
            
        case GUN_AUTO_TUNING:
            if (IS_ON_CRADLE())
            {
                GUN_LOG("Tune ABORTED!\r\n");
                current_state = GUN_COOLING;
            }
            else
            {
                Process_AutoTune();
            }
            break;
    }
}

int Gun_GetRealTemp(void)
{
    return current_real_temp;
}

GunState_t Gun_GetState(void)
{
    return current_state;
}
