#ifndef __IRON_LOGIC_H
#define __IRON_LOGIC_H

#include "py32f0xx_hal.h"

// ==========================================
// ★ 硬件引脚映射 (严格按照你的新表纠正！)
// ==========================================
#define IRON_HEAT_PORT          GPIOB
#define IRON_HEAT_PIN           GPIO_PIN_5  // ★ 烙铁加热移到 PB5

#define IRON_MASTER_SW_PORT     GPIOB
#define IRON_MASTER_SW_PIN      GPIO_PIN_6  // ★ 烙铁总开关是 PB6

typedef enum {
    IRON_OFF = 0,
    IRON_HEATING,
    IRON_SLEEP,
    IRON_ERROR,
    IRON_AUTO_TUNING    // 必须有这个自整定状态
} IronState_t;

typedef struct {
    int temp;
    uint16_t adc;
} IronCalPoint_t;

// ★ 烙铁专属 PID 参数结构体
typedef struct {
    float Kp;
    float Ki;
    float Kd;
} IronPID_Config_t;

extern IronPID_Config_t iron_pid; // 暴露全局变量

void Iron_Init(void);
void Iron_Process(int target_temp);
int Iron_GetRealTemp(void);
IronState_t Iron_GetState(void);
void Iron_Vibration_Trigger(void);

// 触发烙铁 PID 自整定
void Iron_Start_AutoTune(int target_temp);

#endif
