#ifndef __GUN_LOGIC_H
#define __GUN_LOGIC_H

#include "py32f0xx_hal.h"

// ==========================================
// ★ 硬件引脚宏定义
// ==========================================
#define GUN_FAN_PORT        GPIOB
#define GUN_FAN_PIN         GPIO_PIN_2

#define GUN_HEAT_PORT       GPIOB
#define GUN_HEAT_PIN        GPIO_PIN_7

#define GUN_REED_PORT       GPIOA
#define GUN_REED_PIN        GPIO_PIN_8

// ==========================================
// ★ 风枪专属 PID 参数结构体 (彻底解决命名冲突)
// ==========================================
typedef struct {
    float Kp;
    float Ki;
    float Kd;
} GunPID_Config_t;

extern GunPID_Config_t gun_pid; // 暴露全局变量

typedef enum {
    GUN_STANDBY = 0,
    GUN_RUNNING,
    GUN_COOLING,
    GUN_ERROR,
    GUN_AUTO_TUNING   // 自整定状态
} GunState_t;

void Gun_Init(void);
void Gun_Process(int target_temp);
int Gun_GetRealTemp(void);
GunState_t Gun_GetState(void);

// 触发自整定大招
void Gun_Start_AutoTune(int tune_target_temp);

#endif
