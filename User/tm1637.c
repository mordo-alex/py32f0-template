#include "tm1637.h"
#include "board_config.h"
#include <stdio.h> 
#include "advanced_tools.h"

// ==========================================
//  底层 GPIO 及读取宏
// ==========================================
#define CLK_LOW()   HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_RESET)
#define CLK_HIGH()  HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_SET)
#define DIO_LOW()   HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_RESET)
#define DIO_HIGH()  HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_SET)
#define DIO_READ()  HAL_GPIO_ReadPin(TM1637_DIO_PORT, TM1637_DIO_PIN)

static void TM1637_Delay(void) { for(volatile int i = 0; i < 50; i++) { __NOP(); } }

// ==========================================
//  业务全局变量 & 状态机控制
// ==========================================
static int iron_target_temp = 300; 
static int gun_target_temp = 200;  

// ★ 新增：状态机倒计时 (单位: 50ms)
static int iron_adjust_ticks = 0;  // 设定模式倒计时 (例如 20 = 1秒)
static int iron_heating_ticks = 0; // 加热动画倒计时

static int gun_adjust_ticks = 0;   
static int gun_heating_ticks = 0;  

static const uint8_t SegmentMap[] = {
    0x5F, 0x44, 0x9D, 0xD5, 0xC6, 0xD3, 0xDB, 0x45, 0xDF, 0xD7, 0x00, 0x40
};
static uint8_t _brightness = 2;

// ==========================================
//  底层通讯
// ==========================================
void TM1637_Start(void) {
    CLK_HIGH(); DIO_HIGH(); TM1637_Delay();
    DIO_LOW(); TM1637_Delay(); CLK_LOW();
}

void TM1637_Stop(void) {
    CLK_LOW(); TM1637_Delay();
    DIO_LOW(); TM1637_Delay();
    CLK_HIGH(); TM1637_Delay(); DIO_HIGH();
}

void TM1637_WriteByte(uint8_t data) {
    for (uint8_t i = 0; i < 8; i++) {
        CLK_LOW();
        if (data & 0x01) DIO_HIGH(); else DIO_LOW();
        TM1637_Delay();
        CLK_HIGH(); TM1637_Delay();
        data >>= 1;
    }
    CLK_LOW(); DIO_HIGH(); TM1637_Delay(); CLK_HIGH(); TM1637_Delay(); CLK_LOW();
}

uint8_t TM1637_ReadKeys(void) {
    uint8_t key = 0;
    TM1637_Start();
    TM1637_WriteByte(0x42); 
    
    TM1637_DIO_IN(); 
    TM1637_Delay();

    for (int i = 0; i < 8; i++) {
        CLK_LOW(); TM1637_Delay(); 
        key >>= 1;
        CLK_HIGH(); TM1637_Delay(); 
        if (DIO_READ() == GPIO_PIN_SET) { key |= 0x80; }
    }

    TM1637_DIO_OUT(); 
    
    CLK_LOW(); TM1637_Delay();
    DIO_LOW(); TM1637_Delay();
    CLK_HIGH(); TM1637_Delay();
    DIO_HIGH();
    return key;
}

void TM1637_WriteRaw(uint8_t *buff) {
    TM1637_Start(); TM1637_WriteByte(0x40); TM1637_Stop();
    TM1637_Start(); TM1637_WriteByte(0xC0);
    for(int i=0; i<6; i++) TM1637_WriteByte(buff[i]);
    TM1637_Stop();
    TM1637_Start(); TM1637_WriteByte(0x88 | _brightness); TM1637_Stop();
}

// ==========================================
//  ★ 核心显示控制 (真正的 3 阶段状态机)
// ==========================================
void TM1637_Update(void) {
    uint8_t raw_buff[6] = {0}; 
    static uint8_t dp_frame = 0; 
    static uint8_t anim_divider = 0;
    static uint8_t blink_tick = 0; // 新增：用于闪烁动画的节拍器
    
    // --- 全局动画节拍器 ---
    if(++anim_divider >= 2) { 
        anim_divider = 0;
        dp_frame = (dp_frame + 1) % 3; 
        blink_tick = (blink_tick + 1) % 4; // 闪烁节拍：0,1,2,3 循环
    }

    // ==================================================
    // ★ 拦截层：高级工厂菜单霸屏模式
    // ==================================================
    if (sys_menu_active) {
        // 左边显示 F-1/2/3
        raw_buff[2] = 0x71; // 完美渲染大写字母 'F'
        raw_buff[1] = 0x40; // 中间横杠 '-'
        raw_buff[0] = SegmentMap[sys_menu_num]; // 显示当前选中的功能号
        
        // 右边风枪显示灭掉，保持屏幕专注
        raw_buff[5] = 0x00; 
        raw_buff[3] = 0x00; 
        raw_buff[4] = 0x00;

        // ★ 小数点状态机特效
        if (sys_anim_state == ANIM_CYCLING) {
            // 正在处理：跑马灯循环
            if (dp_frame == 0) raw_buff[2] |= 0x20;
            if (dp_frame == 1) raw_buff[1] |= 0x20;
            if (dp_frame == 2) raw_buff[0] |= 0x20;
        } 
        else if (sys_anim_state == ANIM_FLASHING) {
            // 处理完成：齐闪 3 下 (通过 blink_tick < 2 控制亮半周期，灭半周期)
            if (blink_tick < 2) {
                raw_buff[2] |= 0x20; raw_buff[1] |= 0x20; raw_buff[0] |= 0x20;
            }
        } 
        else if (sys_anim_state == ANIM_SOLID) {
            // 最终锁定：3个小数点全部常亮
            raw_buff[2] |= 0x20; raw_buff[1] |= 0x20; raw_buff[0] |= 0x20;
        }

        TM1637_WriteRaw(raw_buff);
        return; // ★ 拦截成功，直接返回！阻止下方的正常渲染！
    }

    // ==================================================
    // ★ 正常工作模式：数值与调整渲染 (保留你的原版逻辑)
    // ==================================================
    raw_buff[2] = SegmentMap[iron_target_temp / 100];
    raw_buff[1] = SegmentMap[(iron_target_temp / 10) % 10];
    raw_buff[0] = SegmentMap[iron_target_temp % 10];
    
    raw_buff[5] = SegmentMap[gun_target_temp / 100];
    raw_buff[3] = SegmentMap[(gun_target_temp / 10) % 10];
    raw_buff[4] = SegmentMap[gun_target_temp % 10];

    // --- 烙铁(左) 状态机 ---
    if (iron_adjust_ticks > 0) {
        // 【状态1: 设定中】倒计时 1 秒。小数点全灭。
        iron_adjust_ticks--;
        if (iron_adjust_ticks == 0) {
            // 设定时间结束，触发加热动作！
            iron_heating_ticks = 60; // 模拟加热 3 秒
        }
    } else if (iron_heating_ticks > 0) {
        // 【状态2: 加热中】PID 介入，跑马灯转起来！
        iron_heating_ticks--;
        if(dp_frame == 0) raw_buff[2] |= 0x20;
        if(dp_frame == 1) raw_buff[1] |= 0x20;
        if(dp_frame == 2) raw_buff[0] |= 0x20;
    } else {
        // 【状态3: 恒温中】PID 稳定，最后一个灯常亮！
        raw_buff[0] |= 0x20;
    }

    // --- 风枪(右) 状态机 ---
    if (gun_adjust_ticks > 0) {
        gun_adjust_ticks--;
        if (gun_adjust_ticks == 0) {
            gun_heating_ticks = 60; 
        }
    } else if (gun_heating_ticks > 0) {
        gun_heating_ticks--;
        if(dp_frame == 0) raw_buff[5] |= 0x20;
        if(dp_frame == 1) raw_buff[3] |= 0x20;
        if(dp_frame == 2) raw_buff[4] |= 0x20;
    } else {
        raw_buff[4] |= 0x20;
    }

    TM1637_WriteRaw(raw_buff);
}

void TM1637_Init(void) { CLK_HIGH(); DIO_HIGH(); }

// ==========================================
//  UI 动作分发 (分离了动画逻辑，这里只管数值)
// ==========================================
void TM1637_ProcessUI(void) {
    static uint8_t last_valid_key = 0xFF;
    static uint16_t key_hold_ticks = 0; 
    
    uint8_t curr_key = TM1637_ReadKeys();
    uint8_t action_key = 0xFF; 

    if (curr_key != 0xFF && curr_key != 0x00) {
        if (curr_key == last_valid_key) {
            key_hold_ticks++;
            if (key_hold_ticks == 1) {
                action_key = curr_key;
            } else if (key_hold_ticks > 10) { 
                if (key_hold_ticks % 2 == 0) {
                    action_key = curr_key;
                }
            }
        } else {
            last_valid_key = curr_key;
            key_hold_ticks = 1;
            action_key = curr_key;
        }
    } else {
        last_valid_key = 0xFF;
        key_hold_ticks = 0;
    }

    if (action_key != 0xFF) {
        switch (action_key) {
            case 0xEC: // 左上
                iron_target_temp += 1;
                if(iron_target_temp > 450) iron_target_temp = 450;
                iron_adjust_ticks = 20; // ★ 重置 1 秒设定延时，暂停加热动画！
                break;
            case 0xED: // 左下
                iron_target_temp -= 1;
                if(iron_target_temp < 50) iron_target_temp = 50;
                iron_adjust_ticks = 20; // ★ 重置 1 秒设定延时
                break;
            case 0xF0: // 右上
                gun_target_temp += 1;
                if(gun_target_temp > 450) gun_target_temp = 450;
                gun_adjust_ticks = 20; 
                break;
            case 0xF5: // 右下
                gun_target_temp -= 1;
                if(gun_target_temp < 50) gun_target_temp = 50;
                gun_adjust_ticks = 20; 
                break;
            default:
                break;
        }
    }

    TM1637_Update();
}
