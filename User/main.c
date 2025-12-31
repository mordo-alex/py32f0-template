#include "py32f0xx_bsp_printf.h"
#include "board_config.h"
#include "iron_pid.h"
#include "gun_logic.h"
#include "tm1637.h"
#include "settings.h"

// ============================================================
// 全局变量定义
// ============================================================
PIDController ironPID;
// PIDController gunPID; // 后续给风枪加 PID

// 延时保存相关变量
uint32_t last_key_action_time = 0;
bool settings_changed = false;

// 按键状态机变量
uint8_t last_key = 0xFF;
uint32_t key_press_time = 0;
uint32_t key_repeat_time = 0;

// ★★★ 请务必通过串口测试后，修改这两个值！★★★
#define KEY_CODE_UP    0xF6  // 示例值：上键键码
#define KEY_CODE_DOWN  0xF2  // 示例值：下键键码

// ============================================================
// 辅助函数：处理按键逻辑 (短按+1, 长按连加)
// ============================================================
void Handle_Buttons(bool iron_on, bool gun_on) 
{
    uint8_t key = TM1637_ReadKeys();
    bool key_triggered = false; // 标记本次循环是否有有效触发
    
    // 调试用：按下按键时打印键值 (帮你确定 KEY_CODE_UP/DOWN)
    if(key != 0xFF && key != last_key) {
        printf("Key Pressed: 0x%02X\r\n", key); 
    }

    // 1. 按键松开处理
    if (key == 0xFF) { 
        last_key = 0xFF;
        return;
    }

    // 2. 按键按下处理
    if (key != last_key) {
        // --- 刚刚按下 (短按) ---
        last_key = key;
        key_press_time = HAL_GetTick();
        key_repeat_time = HAL_GetTick();
        key_triggered = true; // 触发一次
    } 
    else {
        // --- 持续按住 (长按处理) ---
        // 按住超过 500ms 后，每 100ms 触发一次
        if ((HAL_GetTick() - key_press_time > 500) && 
            (HAL_GetTick() - key_repeat_time > 100)) 
        {
            key_repeat_time = HAL_GetTick();
            key_triggered = true; // 触发连发
        }
    }

    // 3. 执行动作 (修改目标温度)
    if (key_triggered) {
        int step = (HAL_GetTick() - key_press_time > 500) ? 5 : 1; // 长按步进5，短按步进1

        if (key == KEY_CODE_UP) {
            if (iron_on) sys_settings.iron_target += step;
            if (gun_on)  sys_settings.gun_target += step;
        } 
        else if (key == KEY_CODE_DOWN) {
            if (iron_on) sys_settings.iron_target -= step;
            if (gun_on)  sys_settings.gun_target -= step;
        }

        // 限制范围 (100度 - 480度)
        if (sys_settings.iron_target > 480) sys_settings.iron_target = 480;
        if (sys_settings.iron_target < 100) sys_settings.iron_target = 100;
        if (sys_settings.gun_target > 480)  sys_settings.gun_target = 480;
        if (sys_settings.gun_target < 100)  sys_settings.gun_target = 100;

        // 标记数据已变更，重置保存倒计时
        settings_changed = true;
        last_key_action_time = HAL_GetTick();
        
        printf("Set: Iron=%d, Gun=%d\r\n", sys_settings.iron_target, sys_settings.gun_target);
    }
}

// ============================================================
// 主函数
// ============================================================
int main(void)
{
    HAL_Init(); // 必须保留，初始化 HAL 库 tick

    // 1. 硬件总初始化 (GPIO, PWM, ADC, 串口, 时钟)
    Board_Init();
    
    // 2. 屏幕初始化
    TM1637_Init();

    // 3. 加载掉电记忆 (如果没有记录则加载默认值 300/350)
    Settings_Load();

    // 4. 逻辑初始化
    Gun_FSM_Init();
    PID_Init(&ironPID);
    ironPID.Kp = 2.0;
    ironPID.Ki = 0.5;
    ironPID.Kd = 0.1;
    ironPID.limMax = 1000; // PWM 周期 1000
    ironPID.T = 0.05;      // 50ms 运行一次

    printf("System Ready! Iron Set: %d, Gun Set: %d\r\n", sys_settings.iron_target, sys_settings.gun_target);

    while (1)
    {
        // ===========================
        // 1. 读取硬件状态
        // ===========================
        uint16_t iron_adc = Board_ADC_Read(ADC_CH_IRON_TEMP);
        uint16_t gun_adc  = Board_ADC_Read(ADC_CH_GUN_TEMP);
        
        // 读开关 (低电平有效 -> 转换为 true/false)
        bool sw_iron_on = (READ_IRON_SW() == 0);
        bool sw_gun_on  = (READ_GUN_SW() == 0);
        // 磁控逻辑: 假设架子上(有磁铁)=吸合=0(低电平); 拿起=断开=1(高电平)
        bool gun_handle_up = (READ_GUN_REED() != 0); 

        // ===========================
        // 2. 处理按键 & 掉电保存
        // ===========================
        Handle_Buttons(sw_iron_on, sw_gun_on);

        // 自动保存逻辑：数据变过 且 停手超过3秒 -> 写 Flash
        if (settings_changed && (HAL_GetTick() - last_key_action_time > 3000)) {
            Settings_Save();
            settings_changed = false;
        }

        // ===========================
        // 3. 烙铁控制逻辑 (PID)
        // ===========================
        int display_iron_val; // 屏幕显示值

        if (sw_iron_on) {
            // 运行 PID: 目标值来自 sys_settings，测量值来自 ADC
            // 注意：这里暂时直接把 ADC 值当温度用，以后要把 ADC 换算成摄氏度
            double pwm = PID_Compute(&ironPID, (double)sys_settings.iron_target, (double)iron_adc); // FIXME: ADC转温度
            Board_Iron_SetPWM((uint16_t)pwm);
            
            // 正常显示实测值 (除4是因为 ADC 12位 4096，大致对应 0-999 显示范围)
            display_iron_val = iron_adc / 4; 
        } else {
            // 关机：停 PWM，复位 PID
            Board_Iron_SetPWM(0);
            PID_Init(&ironPID);
            display_iron_val = -1; // -1 代表灭灯/OFF
        }

        // ===========================
        // 4. 风枪控制逻辑 (状态机)
        // ===========================
        GunInputs_t gun_in;
        gun_in.current_temp = gun_adc / 4; // FIXME: 这里也暂时用 ADC/4 代替摄氏度
        gun_in.sw_is_on = sw_gun_on;
        gun_in.handle_is_up = gun_handle_up;
        gun_in.fan_locked = 0;

        GunOutputs_t gun_out = Gun_FSM_Run(&gun_in);

        // 执行风枪输出
        if (gun_out.fan_on) GUN_FAN_ON(); else GUN_FAN_OFF();
        
        if (gun_out.heat_enable) {
            HAL_GPIO_WritePin(GUN_HEATER_PORT, GUN_HEATER_PIN, GPIO_PIN_RESET); // 开加热
        } else {
            HAL_GPIO_WritePin(GUN_HEATER_PORT, GUN_HEATER_PIN, GPIO_PIN_SET);   // 关加热
        }

        // 准备风枪显示数据
        int display_gun_val;
        if (gun_out.state == GUN_STATE_OFF) {
            display_gun_val = -1; // 关机不显示
        } else {
            display_gun_val = gun_adc / 4; // 显示实测温度 (冷却时也显示)
        }

        // ===========================
        // 5. 屏幕显示刷新
        // ===========================
        // 交互优化：如果在调节按键，显示【设定值】；如果没动按键，显示【实测值】
        if (HAL_GetTick() - last_key_action_time < 2000) {
            // 正在调节：显示设定值
            if (sw_iron_on) display_iron_val = sys_settings.iron_target;
            if (sw_gun_on)  display_gun_val  = sys_settings.gun_target;
        }

        TM1637_Update(display_iron_val, display_gun_val);

        // ===========================
        // 6. 循环延时
        // ===========================
        // 50ms 延时保证按键扫描频率足够 (20Hz)
        HAL_Delay(50); 
    }
}

// 错误处理函数 (必须保留)
void APP_ErrorHandler(void)
{
    while (1);
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    while (1);
}
#endif
