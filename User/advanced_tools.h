#ifndef __ADVANCED_TOOLS_H
#define __ADVANCED_TOOLS_H

#include "py32f0xx_hal.h"

// 菜单动画状态机
typedef enum {
    ANIM_NONE = 0,
    ANIM_CYCLING,
    ANIM_FLASHING,
    ANIM_SOLID
} AnimState_t;

// 暴漏给显示层的只读状态
extern uint8_t sys_menu_active; 
extern uint8_t sys_menu_num;    
extern AnimState_t sys_anim_state;

// ★ 纯血解耦的 API 接口
void Advanced_Tools_Init(void);
void Advanced_Menu_Enable(void);        // 开启菜单
void Advanced_Menu_Disable(void);       // 关闭菜单
void Advanced_Menu_Navigate(int step);  // 翻页 (1 或 -1)
void Advanced_Menu_Confirm(void);       // 确认执行
void Advanced_Menu_ResetIdle(void);     // 重置超时防呆计数器
void Advanced_Menu_Tick(void);          // 放在主循环跑动画和超时
void Advanced_Notify_Job_Done(void);    // 底层任务完成回调

#endif
