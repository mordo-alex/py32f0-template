#ifndef __GUN_LOGIC_H
#define __GUN_LOGIC_H

#include <stdint.h>
#include <stdbool.h>

// 风枪的状态定义
typedef enum {
    GUN_STATE_OFF = 0,      // 彻底关机 (风扇停，加热停)
    GUN_STATE_HEATING,      // 正常工作 (风扇转，PID控制)
    GUN_STATE_COOLING,      // 冷却模式 (风扇转，加热停) - 比如放回架子，或关机后余热未散
    GUN_STATE_ERROR         // 故障 (如风扇堵转)
} GunState_t;

// 输入信号 (传感器 + 开关)
typedef struct {
    uint16_t current_temp;  // 当前温度 (ADC值或摄氏度)
    bool     sw_is_on;      // 总开关是否开启 (1=开, 0=关)
    bool     handle_is_up;  // 手柄是否拿起来 (1=拿起, 0=在架子上)
    bool     fan_locked;    // (选配) 风扇是否堵转
} GunInputs_t;

// 输出控制 (告诉 main 函数该干嘛)
typedef struct {
    bool     fan_on;        // 是否开风扇
    bool     heat_enable;   // 是否允许加热 (PID计算的前提)
    GunState_t state;       // 当前状态 (用于显示屏判断显示内容)
} GunOutputs_t;

// 核心函数
void Gun_FSM_Init(void);
GunOutputs_t Gun_FSM_Run(GunInputs_t *inputs);

#endif
