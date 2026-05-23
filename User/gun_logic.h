#ifndef __GUN_LOGIC_H
#define __GUN_LOGIC_H

#include "py32f0xx_hal.h"

// ==========================================
// ★ 宏控开关：1 开启风枪调试日志，0 关闭
// ==========================================
#define GUN_DEBUG_EN        1

#define GUN_FAN_PORT        GPIOB
#define GUN_FAN_PIN         GPIO_PIN_2

#define GUN_HEAT_PORT       GPIOB
#define GUN_HEAT_PIN        GPIO_PIN_7

#define GUN_MASTER_SW_PORT  GPIOA
#define GUN_MASTER_SW_PIN   GPIO_PIN_8  

#define GUN_REED_PORT       GPIOB
#define GUN_REED_PIN        GPIO_PIN_4  

typedef struct {
    float Kp;
    float Ki;
    float Kd;
} GunPID_Config_t;

extern GunPID_Config_t gun_pid; 

typedef enum {
    GUN_OFF = 0,
    GUN_STANDBY,
    GUN_RUNNING,
    GUN_COOLING,
    GUN_ERROR,
    GUN_AUTO_TUNING   
} GunState_t;

void Gun_Init(void);
void Gun_Process(int target_temp);
int Gun_GetRealTemp(void);
GunState_t Gun_GetState(void);
void Gun_Start_AutoTune(int tune_target_temp);

#endif
