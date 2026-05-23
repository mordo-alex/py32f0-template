#include "py32f0xx_bsp_printf.h"
#include "board_config.h"
#include "tm1637.h"
#include "adc_core.h"
#include "iron_logic.h"
#include "gun_logic.h"
#include "advanced_tools.h"
#include "settings.h" 
#include <stdbool.h>  // ★ 致命修复：引入 bool 类型头文件！

void UI_Controller_Task(void) {
    static uint8_t prev_raw_key = 0xFF;
    static uint8_t seq_step = 0;
    static uint32_t last_seq_tick = 0; // 密码锁超时计时
    static uint32_t last_key_tick = 0;
    static uint8_t show_target_ticks = 0; 
    static bool settings_changed = false;
    
    uint8_t curr_key = TM1637_ReadKeys();
    uint8_t action_key = 0xFF;
    uint8_t click_key = 0xFF;

    // 1. 按键解析 (单发与长按连发分离)
    if (curr_key != prev_raw_key) {
        if (curr_key != 0xFF && curr_key != 0x00) { 
            action_key = curr_key; 
            click_key = curr_key; 
            last_key_tick = HAL_GetTick(); 
        }
        prev_raw_key = curr_key;
    } else if (curr_key != 0xFF && curr_key != 0x00) {
        if (HAL_GetTick() - last_key_tick > 400) { // 长按 400ms 后触发连发
            action_key = curr_key; 
            last_key_tick = HAL_GetTick() - 300; 
        }
    }

    // 2. 业务路由
    if (!sys_menu_active) {
        
        // 2.1 ★ Z字防呆密码锁检测 (修复：恢复密码锁逻辑)
        if (Iron_GetState() == IRON_OFF && Gun_GetState() == GUN_OFF) {
            if (click_key != 0xFF) {
                if      (seq_step == 0 && click_key == 0xEC) { seq_step = 1; last_seq_tick = HAL_GetTick(); }
                else if (seq_step == 1 && click_key == 0xED) { seq_step = 2; last_seq_tick = HAL_GetTick(); }
                else if (seq_step == 2 && click_key == 0xF0) { seq_step = 3; last_seq_tick = HAL_GetTick(); }
                else if (seq_step == 3 && click_key == 0xF5) { seq_step = 0; Advanced_Menu_Enable(); action_key = 0xFF; }
                else { seq_step = 0; } 
            }
            // 密码输入超时复位
            if (seq_step > 0 && (HAL_GetTick() - last_seq_tick > 2000)) seq_step = 0;
        } else {
            seq_step = 0;
        }

        // 2.2 正常调温处理
        if (action_key != 0xFF && seq_step == 0) {
            if (Iron_GetState() != IRON_OFF) { 
                if (action_key == 0xEC) { sys_settings.iron_target += 1; show_target_ticks = 30; settings_changed = true; } // 改回 1
                if (action_key == 0xED) { sys_settings.iron_target -= 1; show_target_ticks = 30; settings_changed = true; } // 改回 1
            }
            if (Gun_GetState() != GUN_OFF) { 
                if (action_key == 0xF0) { sys_settings.gun_target += 1; show_target_ticks = 30; settings_changed = true; }  // 改回 1
                if (action_key == 0xF5) { sys_settings.gun_target -= 1; show_target_ticks = 30; settings_changed = true; }  // 改回 1
            }
            
            if(sys_settings.iron_target > 480) { sys_settings.iron_target = 480; }
            if(sys_settings.iron_target < 100) { sys_settings.iron_target = 100; }
            if(sys_settings.gun_target > 480)  { sys_settings.gun_target = 480;  }
            if(sys_settings.gun_target < 100)  { sys_settings.gun_target = 100;  }
        } 
    } else {
        // 2.3 高级菜单内逻辑
        if (Iron_GetState() != IRON_OFF || Gun_GetState() != GUN_OFF) {
            Advanced_Menu_Disable(); // 有设备工作时强制退出菜单
        } else {
            if (click_key != 0xFF) {
                Advanced_Menu_ResetIdle();
                if (click_key == 0xEC) Advanced_Menu_Navigate(1);     
                if (click_key == 0xED) Advanced_Menu_Navigate(-1);    
                if (click_key == 0xF0) Advanced_Menu_Confirm();       
                if (click_key == 0xF5) Advanced_Menu_Disable();       
            }
            Advanced_Menu_Tick(); 
        }
    }

    // 自动保存 (按键停止 3 秒后保存)
    if (settings_changed && (HAL_GetTick() - last_key_tick > 3000)) {
        Settings_Save(); 
        settings_changed = false;
    }

    // 3. 装配 UI 结构体发给 View 层
    UI_DisplayData_t disp = {0};
    if (show_target_ticks > 0) show_target_ticks--;

    if (sys_menu_active) {
        disp.iron_mode = UI_MODE_MENU; 
        disp.iron_val = sys_menu_num; 
        
        if (sys_anim_state == ANIM_CYCLING) disp.iron_anim = 1;
        else if (sys_anim_state == ANIM_FLASHING) disp.iron_anim = 2;
        else disp.iron_anim = 0;
        
        disp.gun_mode = UI_MODE_OFF;
    } else {
        // --- 组装烙铁显示 ---
        if (Iron_GetState() == IRON_OFF) { disp.iron_mode = UI_MODE_OFF; }
        else if (Iron_GetState() == IRON_ERROR) { disp.iron_mode = UI_MODE_ERROR; }
        else {
            disp.iron_mode = UI_MODE_NORMAL;
            disp.iron_val = (show_target_ticks > 0) ? sys_settings.iron_target : Iron_GetRealTemp();
            if (Iron_GetState() == IRON_HEATING) disp.iron_anim = 1;
        }

        // --- 组装风枪显示 ---
        if (Gun_GetState() == GUN_OFF) { disp.gun_mode = UI_MODE_OFF; }
        else if (Gun_GetState() == GUN_ERROR) { disp.gun_mode = UI_MODE_ERROR; }
        else {
            disp.gun_mode = UI_MODE_NORMAL;
            disp.gun_val = (show_target_ticks > 0) ? sys_settings.gun_target : Gun_GetRealTemp();
            if (Gun_GetState() == GUN_RUNNING) disp.gun_anim = 1;
        }
    }

    // 4. 调用纯粹的渲染接口
    TM1637_UpdateDisplay(&disp);
}

int main(void)
{
    HAL_Init();
    Board_Init();
    BSP_USART_Config();

    printf("\r\n=================================\r\n");
    printf("  Dual Station: MVC Booting      \r\n");
    
    Settings_Load();
    TM1637_Init();         
    ADC_Core_Init();        
    Iron_Init();            
    Gun_Init();             
    Advanced_Tools_Init();  

    printf("=================================\r\n");

    while (1)
    {
        // 1. 核心业务运转
        Iron_Process(sys_settings.iron_target);
        Gun_Process(sys_settings.gun_target);

        // 2. 交互与 UI 渲染调度
        UI_Controller_Task();

        HAL_Delay(50);
    }
}

void APP_ErrorHandler(void) { while (1); }
