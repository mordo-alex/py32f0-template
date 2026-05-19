#ifndef __ADVANCED_TOOLS_H
#define __ADVANCED_TOOLS_H

#include "py32f0xx_hal.h"

// 菜单动画状态机枚举
typedef enum {
    ANIM_NONE = 0,      // 菜单选择状态
    ANIM_CYCLING,       // 正在执行：小数点跑马灯循环
    ANIM_FLASHING,      // 执行完毕：3个小数点同时闪烁
    ANIM_SOLID          // 最终锁定：3个小数点常亮
} AnimState_t;

// 暴漏给 tm1637.c 用于屏幕接管的全局变量
extern uint8_t sys_menu_active; // 0=正常, 1=进入高级菜单
extern uint8_t sys_menu_num;    // 1=F-1, 2=F-2, 3=F-3
extern AnimState_t sys_anim_state;

void Advanced_Tools_Init(void);
void Advanced_Menu_Task(uint8_t current_key);
void Advanced_Notify_Job_Done(void); // 外部任务完成时调用

#endif
