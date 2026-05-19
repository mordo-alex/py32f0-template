#include "advanced_tools.h"
#include "adc_core.h"
#include "iron_logic.h"
#include "gun_logic.h"
#include "py32f0xx_bsp_printf.h"
#include <stdlib.h>

#define KEY_EMPTY      0xFF
#define KEY_IRON_UP    0xEC
#define KEY_IRON_DOWN  0xED
#define KEY_GUN_UP     0xF0
#define KEY_GUN_DOWN   0xF5

uint8_t sys_menu_active = 0;
uint8_t sys_menu_num = 1;
AnimState_t sys_anim_state = ANIM_NONE;

static uint16_t anim_ticks = 0;
static uint32_t menu_idle_ticks = 0; // ★ 新增：菜单空闲超时计数器

void Advanced_Tools_Init(void) {
    sys_menu_active = 0;
    sys_menu_num = 1;
    sys_anim_state = ANIM_NONE;
}

void Advanced_Notify_Job_Done(void) {
    sys_anim_state = ANIM_FLASHING;
    anim_ticks = 0;
    printf("[qiang@PY32] TASK F-%d DONE! Flashing animation started.\r\n", sys_menu_num);
}

/**
  * @brief 高级菜单任务 (每50ms在主循环中调用)
  */
void Advanced_Menu_Task(uint8_t current_key) {
    extern uint32_t system_ticks; 
    static uint32_t last_iron_up_tick = 0;
    static uint32_t last_gun_down_tick = 0;
    static uint32_t last_iron_down_tick = 0;
    static uint16_t enter_menu_hold_cnt = 0;
    static uint16_t confirm_hold_cnt = 0;
    static uint8_t last_key_processed = 0xFF;

    // 0. 更新按键时间戳
    if (current_key == KEY_IRON_UP)   last_iron_up_tick = system_ticks;
    if (current_key == KEY_GUN_DOWN)  last_gun_down_tick = system_ticks;
    if (current_key == KEY_IRON_DOWN) last_iron_down_tick = system_ticks;

    // ★ 只要有任何按键动作，重置超时计数器
    if (current_key != KEY_EMPTY && current_key != 0x00) {
        menu_idle_ticks = 0;
    }

    // ==================================================
    // ★ 全局拦截器：进入 / 退出 菜单 (双键长按)
    // ==================================================
    // 判断: 烙铁(+) 和 风枪(-) 是否同时按下
    if (abs((int)last_iron_up_tick - (int)last_gun_down_tick) <= 3 && 
        (system_ticks - last_iron_up_tick) <= 3) 
    {
        enter_menu_hold_cnt++;
        if (enter_menu_hold_cnt > 40) { // 持续按住 2 秒 (40 * 50ms)
            enter_menu_hold_cnt = 0;
            
            // 状态翻转：进菜单 或 强制退出菜单
            if (!sys_menu_active) {
                sys_menu_active = 1;
                sys_menu_num = 1;
                sys_anim_state = ANIM_NONE;
                menu_idle_ticks = 0; // 刚进来，清空超时
                printf("\r\n=================================\r\n");
                printf("[qiang@PY32] FACTORY MENU ENTERED! \r\n");
                printf("=================================\r\n");
            } else {
                sys_menu_active = 0;
                printf("[qiang@PY32] USER MANUAL EXIT. Back to normal.\r\n");
            }
            return; // 执行完毕，直接拦截本次循环
        }
    } else {
        enter_menu_hold_cnt = 0; // 松手则清零
    }

    // 如果没在菜单里，直接退出函数，不跑后续逻辑
    if (!sys_menu_active) return;

    // ==================================================
    // ★ 菜单无操作超时防呆机制 (10秒没按键，自动跑路)
    // ==================================================
    // 只有在选单模式下才计算超时（别在跑 PID 的时候退出了）
    if (sys_anim_state == ANIM_NONE) {
        menu_idle_ticks++;
        if (menu_idle_ticks > 200) { // 200 * 50ms = 10秒
            sys_menu_active = 0;
            printf("[qiang@PY32] MENU TIMEOUT (10s idle). Auto Exit.\r\n");
            return;
        }
    }

    // ==================================================
    // ★ 菜单内部操作与动画流转
    // ==================================================
    
    // A. 选单模式
    if (sys_anim_state == ANIM_NONE) {
        // 1. 切换菜单项 (单击)
        if (current_key != last_key_processed) {
            if (current_key == KEY_IRON_UP || current_key == KEY_GUN_UP) {
                sys_menu_num++; if (sys_menu_num > 3) sys_menu_num = 1;
                printf("[qiang@PY32] Menu Selected: F-%d\r\n", sys_menu_num);
            }
            if (current_key == KEY_IRON_DOWN || current_key == KEY_GUN_DOWN) {
                sys_menu_num--; if (sys_menu_num < 1) sys_menu_num = 3;
                printf("[qiang@PY32] Menu Selected: F-%d\r\n", sys_menu_num);
            }
            last_key_processed = current_key;
        }

        // 2. 确认执行 (同时长按 上+下 1秒)
        if (abs((int)last_iron_up_tick - (int)last_iron_down_tick) <= 3 && 
            (system_ticks - last_iron_up_tick) <= 3) 
        {
            confirm_hold_cnt++;
            if (confirm_hold_cnt > 20) { 
                sys_anim_state = ANIM_CYCLING; // 触发跑马灯！
                anim_ticks = 0;
                confirm_hold_cnt = 0;
                
                printf("\r\n[qiang@PY32] ---> EXECUTING F-%d < ---\r\n", sys_menu_num);
                // ★ 派发底层任务！
                if (sys_menu_num == 1) ADC_Factory_Calibrate_Zero();
                else if (sys_menu_num == 2) Iron_Start_AutoTune(300);
                else if (sys_menu_num == 3) Gun_Start_AutoTune(300);
            }
        } else {
            confirm_hold_cnt = 0;
        }
    }

    // B. 动画引擎流转
    if (sys_anim_state == ANIM_CYCLING) {
        anim_ticks++;
        // TODO: 这里是模拟！执行 3 秒后强制认为完成。
        // 等你联调底层时，把下面这个 if 注释掉，靠底层调用 Advanced_Notify_Job_Done() 来推进！
        if (anim_ticks > 60 && sys_menu_num == 1) { Advanced_Notify_Job_Done(); }
    } 
    else if (sys_anim_state == ANIM_FLASHING) {
        anim_ticks++;
        if (anim_ticks > 30) { // 闪烁 1.5 秒
            sys_anim_state = ANIM_SOLID;
            anim_ticks = 0;
            printf("[qiang@PY32] Animation: Solid lock for 2 seconds.\r\n");
        }
    }
    else if (sys_anim_state == ANIM_SOLID) {
        anim_ticks++;
        if (anim_ticks > 40) { // 常亮锁定 2 秒 (40 * 50ms) 后...
            // ★ 核心改动：不退出菜单，而是退回到选单状态！
            sys_anim_state = ANIM_NONE; 
            menu_idle_ticks = 0; // 重置超时
            printf("[qiang@PY32] Task finished! Returned to menu selection.\r\n");
        }
    }

    if (current_key == KEY_EMPTY) last_key_processed = 0xFF;
}
