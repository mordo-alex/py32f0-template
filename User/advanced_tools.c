#include "advanced_tools.h"
#include "adc_core.h"
#include "iron_logic.h"
#include "gun_logic.h"
#include "py32f0xx_bsp_printf.h"

uint8_t sys_menu_active = 0;
uint8_t sys_menu_num = 1;
AnimState_t sys_anim_state = ANIM_NONE;

static uint16_t anim_ticks = 0;
static uint32_t menu_idle_ticks = 0; 

void Advanced_Tools_Init(void) {
    sys_menu_active = 0; sys_menu_num = 1; sys_anim_state = ANIM_NONE;
}

void Advanced_Notify_Job_Done(void) {
    sys_anim_state = ANIM_FLASHING; anim_ticks = 0;
    printf("[Menu] TASK F-%d DONE!\r\n", sys_menu_num);
}

void Advanced_Menu_Enable(void) {
    sys_menu_active = 1; sys_menu_num = 1; sys_anim_state = ANIM_NONE; menu_idle_ticks = 0;
    printf("\r\n[Menu] === FACTORY MENU ENTERED ===\r\n");
}

void Advanced_Menu_Disable(void) {
    sys_menu_active = 0;
    printf("[Menu] === EXIT ===\r\n");
}

void Advanced_Menu_Navigate(int step) {
    if (sys_anim_state != ANIM_NONE) return; // 正在执行时禁止翻页
    if (step > 0) { sys_menu_num++; if (sys_menu_num > 3) sys_menu_num = 1; }
    else          { sys_menu_num--; if (sys_menu_num < 1) sys_menu_num = 3; }
    printf("[Menu] Selected: F-%d\r\n", sys_menu_num);
}

void Advanced_Menu_Confirm(void) {
    if (sys_anim_state != ANIM_NONE) return;
    sys_anim_state = ANIM_CYCLING; anim_ticks = 0;
    printf("[Menu] ---> EXECUTING F-%d < ---\r\n", sys_menu_num);
    
    // 派发底层干活
    if (sys_menu_num == 1) ADC_Factory_Calibrate_Zero();
    else if (sys_menu_num == 2) Iron_Start_AutoTune(300);
    else if (sys_menu_num == 3) Gun_Start_AutoTune(300);
}

void Advanced_Menu_ResetIdle(void) { menu_idle_ticks = 0; }

// ★ 主循环里的心跳机 (负责超时和动画流转)
void Advanced_Menu_Tick(void) {
    if (!sys_menu_active) return;

    if (sys_anim_state == ANIM_NONE) {
        menu_idle_ticks++;
        if (menu_idle_ticks > 200) { // 10秒超时退出
            Advanced_Menu_Disable();
            printf("[Menu] TIMEOUT. Auto Exit.\r\n");
        }
    } 
    else if (sys_anim_state == ANIM_CYCLING) {
        anim_ticks++;
        // TODO: 联调时删掉下面这行，靠底层调 Advanced_Notify_Job_Done 翻转状态
        if (anim_ticks > 60 && sys_menu_num == 1) { Advanced_Notify_Job_Done(); } 
    } 
    else if (sys_anim_state == ANIM_FLASHING) {
        anim_ticks++; if (anim_ticks > 30) { sys_anim_state = ANIM_SOLID; anim_ticks = 0; }
    } 
    else if (sys_anim_state == ANIM_SOLID) {
        anim_ticks++; if (anim_ticks > 40) { sys_anim_state = ANIM_NONE; menu_idle_ticks = 0; printf("[Menu] Task finished!\r\n"); }
    }
}
