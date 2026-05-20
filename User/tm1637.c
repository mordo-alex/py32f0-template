#include "tm1637.h"
#include "board_config.h"
#include <stdio.h>
#include "advanced_tools.h"
#include "iron_logic.h"
#include "gun_logic.h"

#define CLK_LOW()   HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_RESET)
#define CLK_HIGH()  HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_SET)
#define DIO_LOW()   HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_RESET)
#define DIO_HIGH()  HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_SET)
#define DIO_READ()  HAL_GPIO_ReadPin(TM1637_DIO_PORT, TM1637_DIO_PIN)
static void TM1637_Delay(void) { for(volatile int i = 0; i < 50; i++) { __NOP(); } }

extern int iron_target_temp; 
extern int gun_target_temp;
static int iron_adjust_ticks = 0;  static int iron_heating_ticks = 0; 
static int gun_adjust_ticks = 0;   static int gun_heating_ticks = 0;

static const uint8_t SegmentMap[] = { 0x5F, 0x44, 0x9D, 0xD5, 0xC6, 0xD3, 0xDB, 0x45, 0xDF, 0xD7, 0x00, 0x40 };
static uint8_t _brightness = 2;

void TM1637_Start(void) { CLK_HIGH(); DIO_HIGH(); TM1637_Delay(); DIO_LOW(); TM1637_Delay(); CLK_LOW(); }
void TM1637_Stop(void) { CLK_LOW(); TM1637_Delay(); DIO_LOW(); TM1637_Delay(); CLK_HIGH(); TM1637_Delay(); DIO_HIGH(); }
void TM1637_WriteByte(uint8_t data) {
    for (uint8_t i = 0; i < 8; i++) {
        CLK_LOW(); if (data & 0x01) DIO_HIGH(); else DIO_LOW(); TM1637_Delay();
        CLK_HIGH(); TM1637_Delay(); data >>= 1;
    }
    CLK_LOW(); DIO_HIGH(); TM1637_Delay(); CLK_HIGH(); TM1637_Delay(); CLK_LOW();
}

uint8_t TM1637_ReadKeys(void) {
    uint8_t key = 0;
    TM1637_Start(); TM1637_WriteByte(0x42); TM1637_DIO_IN(); TM1637_Delay();
    for (int i = 0; i < 8; i++) {
        CLK_LOW(); TM1637_Delay(); key >>= 1; CLK_HIGH(); TM1637_Delay();
        if (DIO_READ() == GPIO_PIN_SET) { key |= 0x80; }
    }
    TM1637_DIO_OUT(); CLK_LOW(); TM1637_Delay(); DIO_LOW(); TM1637_Delay(); CLK_HIGH(); TM1637_Delay(); DIO_HIGH();
    return key;
}

void TM1637_WriteRaw(uint8_t *buff) {
    TM1637_Start(); TM1637_WriteByte(0x40); TM1637_Stop();
    TM1637_Start(); TM1637_WriteByte(0xC0);
    for(int i=0; i<6; i++) TM1637_WriteByte(buff[i]);
    TM1637_Stop();
    TM1637_Start(); TM1637_WriteByte(0x88 | _brightness); TM1637_Stop();
}

void TM1637_Update(void) {
    uint8_t raw_buff[6] = {0}; 
    static uint8_t dp_frame = 0; static uint8_t anim_divider = 0; static uint8_t blink_tick = 0;
    if(++anim_divider >= 2) { anim_divider = 0; dp_frame = (dp_frame + 1) % 3; blink_tick = (blink_tick + 1) % 4; }

    // ==========================================
    // ★ 拦截层：高级菜单
    // ==========================================
    if (sys_menu_active) {
        // ★ 修复魔改板子的特征码：0x8B = 'F', 0x80 = '-'
        raw_buff[2] = 0x8B; 
        raw_buff[1] = 0x80; 
        raw_buff[0] = SegmentMap[sys_menu_num]; 
        
        raw_buff[5] = 0x00; raw_buff[3] = 0x00; raw_buff[4] = 0x00;
        
        // 小数点动画引擎 (0x20 是小数点的专属码)
        if (sys_anim_state == ANIM_CYCLING) {
            if (dp_frame == 0) { raw_buff[2] |= 0x20; }
            if (dp_frame == 1) { raw_buff[1] |= 0x20; }
            if (dp_frame == 2) { raw_buff[0] |= 0x20; }
        } else if (sys_anim_state == ANIM_FLASHING) {
            if (blink_tick < 2) { raw_buff[2] |= 0x20; raw_buff[1] |= 0x20; raw_buff[0] |= 0x20; }
        } else if (sys_anim_state == ANIM_SOLID) {
            raw_buff[2] |= 0x20; raw_buff[1] |= 0x20; raw_buff[0] |= 0x20;
        }
        TM1637_WriteRaw(raw_buff);
        return; 
    }

    // ==========================================
    // ★ 正常工作层
    // ==========================================
    if (Iron_GetState() == IRON_OFF) {
        raw_buff[2] = 0x00; raw_buff[1] = 0x00; raw_buff[0] = 0x00; 
    } else {
        raw_buff[2] = SegmentMap[iron_target_temp / 100]; raw_buff[1] = SegmentMap[(iron_target_temp / 10) % 10]; raw_buff[0] = SegmentMap[iron_target_temp % 10];
        if (iron_adjust_ticks > 0) { iron_adjust_ticks--; if (iron_adjust_ticks == 0) iron_heating_ticks = 60; } 
        else if (iron_heating_ticks > 0) { iron_heating_ticks--; if(dp_frame == 0) {raw_buff[2] |= 0x20;} if(dp_frame == 1) {raw_buff[1] |= 0x20;} if(dp_frame == 2) {raw_buff[0] |= 0x20;} } 
        else { raw_buff[0] |= 0x20; }
    }

    if (Gun_GetState() == GUN_OFF) { 
        raw_buff[5] = 0x00; raw_buff[3] = 0x00; raw_buff[4] = 0x00; 
    } else {
        raw_buff[5] = SegmentMap[gun_target_temp / 100]; raw_buff[3] = SegmentMap[(gun_target_temp / 10) % 10]; raw_buff[4] = SegmentMap[gun_target_temp % 10];
        if (gun_adjust_ticks > 0) { gun_adjust_ticks--; if (gun_adjust_ticks == 0) gun_heating_ticks = 60; } 
        else if (gun_heating_ticks > 0) { gun_heating_ticks--; if(dp_frame == 0) {raw_buff[5] |= 0x20;} if(dp_frame == 1) {raw_buff[3] |= 0x20;} if(dp_frame == 2) {raw_buff[4] |= 0x20;} } 
        else { raw_buff[4] |= 0x20; }
    }
    TM1637_WriteRaw(raw_buff);
}

void TM1637_Init(void) { CLK_HIGH(); DIO_HIGH(); }

void TM1637_ProcessUI(void) {
    static uint8_t prev_raw_key = 0xFF;
    static uint8_t seq_step = 0;
    static uint32_t last_seq_tick = 0;
    
    uint8_t curr_key = TM1637_ReadKeys();
    uint8_t key_click = 0xFF;

    if (curr_key != prev_raw_key) {
        if (curr_key != 0xFF && curr_key != 0x00) {
            key_click = curr_key; 
            if (sys_menu_active) Advanced_Menu_ResetIdle();
        }
        prev_raw_key = curr_key;
    }

    // ==========================================
    // ★ Z字密码锁：带终极防呆保护
    // ==========================================
    if (!sys_menu_active) {
        // ★ 神级条件：必须烙铁和风枪都处于关闭（OFF）状态，才允许输入密码！
        if (Iron_GetState() == IRON_OFF && Gun_GetState() == GUN_OFF) {
            if (key_click != 0xFF) {
                if      (seq_step == 0 && key_click == 0xEC) { seq_step = 1; last_seq_tick = HAL_GetTick(); }
                else if (seq_step == 1 && key_click == 0xED) { seq_step = 2; last_seq_tick = HAL_GetTick(); }
                else if (seq_step == 2 && key_click == 0xF0) { seq_step = 3; last_seq_tick = HAL_GetTick(); }
                else if (seq_step == 3 && key_click == 0xF5) { 
                    seq_step = 0; 
                    Advanced_Menu_Enable(); 
                }
                else { seq_step = 0; } 
            }
            if (seq_step > 0 && (HAL_GetTick() - last_seq_tick > 2000)) { seq_step = 0; }
        } else {
            seq_step = 0; // 只要有任何一个处于开机状态，直接清空密码进度，绝对不让进！
        }
    } 
    // ==========================================
    // ★ 菜单内部交互
    // ==========================================
    else if (sys_menu_active) {
        // 如果在菜单内，用户突然打开了总开关，强制踢出菜单防炸机！
        if (Iron_GetState() != IRON_OFF || Gun_GetState() != GUN_OFF) {
            Advanced_Menu_Disable();
        } else {
            if (key_click != 0xFF) {
                if (key_click == 0xEC) Advanced_Menu_Navigate(1);     
                if (key_click == 0xED) Advanced_Menu_Navigate(-1);    
                if (key_click == 0xF0) Advanced_Menu_Confirm();       
                if (key_click == 0xF5) Advanced_Menu_Disable();       
            }
            Advanced_Menu_Tick(); 
        }
    } 
    
    // ==========================================
    // ★ 普通调温模式
    // ==========================================
    if (!sys_menu_active && seq_step == 0) {
        static uint16_t key_hold_ticks = 0;
        uint8_t action_key = 0xFF;

        if (curr_key != 0xFF && curr_key != 0x00) {
            key_hold_ticks++;
            if (key_hold_ticks == 1) action_key = curr_key;
            else if (key_hold_ticks > 10 && key_hold_ticks % 2 == 0) action_key = curr_key;
        } else {
            key_hold_ticks = 0;
        }

        if (action_key != 0xFF) {
            // 只有在开机状态下，才允许调节温度
            if (Iron_GetState() != IRON_OFF) {
                if (action_key == 0xEC) { iron_target_temp += 1; if(iron_target_temp > 450) iron_target_temp = 450; iron_adjust_ticks = 20; }
                if (action_key == 0xED) { iron_target_temp -= 1; if(iron_target_temp < 50) iron_target_temp = 50; iron_adjust_ticks = 20; }
            }
            if (Gun_GetState() != GUN_OFF) {
                if (action_key == 0xF0) { gun_target_temp += 1; if(gun_target_temp > 450) gun_target_temp = 450; gun_adjust_ticks = 20; }
                if (action_key == 0xF5) { gun_target_temp -= 1; if(gun_target_temp < 50) gun_target_temp = 50; gun_adjust_ticks = 20; }
            }
        }
    }

    TM1637_Update();
}
