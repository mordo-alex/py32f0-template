#include "gun_logic.h"

// 安全冷却阈值 (比如 ADC 值 200 大约对应 50度，具体看你校准)
// 暂时假设我们传入的是摄氏度，或者 ADC 值。
// 这里假设传入的是【ADC值】，假设室温是 0-100，高温是 2000+
// 为了安全，我们定一个比较低的值，比如 ADC < 100 (接近室温) 才停风扇
#define SAFE_TEMP_THRESHOLD  100 

static GunState_t currentState = GUN_STATE_OFF;

void Gun_FSM_Init(void) {
    currentState = GUN_STATE_OFF;
}

GunOutputs_t Gun_FSM_Run(GunInputs_t *in) {
    GunOutputs_t out = {0};

    switch (currentState) {
        // --- 1. 关机/待机状态 ---
        case GUN_STATE_OFF:
            out.fan_on = false;
            out.heat_enable = false;

            // 状态流转:
            // 如果 [开关开了] 且 [手柄拿起来了] -> 进加热
            if (in->sw_is_on && in->handle_is_up) {
                currentState = GUN_STATE_HEATING;
            }
            // 如果 [开关开了] 但 [手柄在架子上] -> 保持 OFF (或者叫待机)
            // 如果 [余热检测]：哪怕是关机状态，如果检测到温度异常高，也得强行切到冷却
            if (in->current_temp > SAFE_TEMP_THRESHOLD) {
                currentState = GUN_STATE_COOLING;
            }
            break;

        // --- 2. 加热工作状态 ---
        case GUN_STATE_HEATING:
            out.fan_on = true;
            out.heat_enable = true; // 允许 PID 介入

            // 状态流转:
            // 如果 [开关关了] -> 进冷却
            if (!in->sw_is_on) {
                currentState = GUN_STATE_COOLING;
            }
            // 如果 [手柄放回架子] -> 进冷却
            else if (!in->handle_is_up) {
                currentState = GUN_STATE_COOLING;
            }
            break;

        // --- 3. 冷却状态 (最关键的安全逻辑) ---
        case GUN_STATE_COOLING:
            out.fan_on = true;       // 必须吹风！
            out.heat_enable = false; // 严禁加热

            // 状态流转:
            // 只有当 [温度足够低] 时，才允许根据开关状态决定去向
            if (in->current_temp < SAFE_TEMP_THRESHOLD) {
                if (in->sw_is_on && in->handle_is_up) {
                    // 还没凉透你就又拿起来用了 -> 重新加热
                    currentState = GUN_STATE_HEATING;
                } else {
                    // 凉透了，且没用车 -> 彻底关机
                    currentState = GUN_STATE_OFF;
                }
            } 
            // 如果温度还高，就死锁在这个状态，谁也别想关风扇
            else {
                // 如果此时用户又把手柄拿起来准备干活 (且开关开着)
                // 允许立即打断冷却，切回加热 (体验更好)
                if (in->sw_is_on && in->handle_is_up) {
                    currentState = GUN_STATE_HEATING;
                }
            }
            break;
            
        case GUN_STATE_ERROR:
            out.fan_on = false;
            out.heat_enable = false;
            break;
    }

    out.state = currentState;
    return out;
}
