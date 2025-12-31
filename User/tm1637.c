#include "tm1637.h"
#include "board_config.h"

// ==========================================
//  底层 GPIO 操作宏 (依赖 board_config.h)
// ==========================================
#define CLK_LOW()   HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_RESET)
#define CLK_HIGH()  HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_SET)
#define DIO_LOW()   HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_RESET)
#define DIO_HIGH()  HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_SET)

// 延时函数 (TM1637 只要稍微延时一点点即可)
static void TM1637_Delay(void)
{
    for(volatile int i = 0; i < 50; i++) { __NOP(); }
}

// ==========================================
//  1. 特殊段码表 (根据你的图片逆向)
// ==========================================
// TM1637 的位定义: b0=SEG1, b1=SEG2 ... b7=SEG8
// 你的连接:
// SEG1 -> A (b0)
// SEG2 -> F (b1)
// SEG3 -> B (b2)
// SEG4 -> E (b3)
// SEG5 -> D (b4)
// SEG6 -> DP(b5)
// SEG7 -> C (b6)
// SEG8 -> G (b7)

// 构造数字 0-9 的字模:
// 数字 0 (A,B,C,D,E,F亮) -> b0,b2,b6,b4,b3,b1 = 1 -> 0x5F
// 数字 1 (B,C亮)         -> b2,b6          = 1 -> 0x44
// ... 依次类推
static const uint8_t SegmentMap[] = {
    0x5F, // 0
    0x44, // 1
    0x9D, // 2 (A,B,D,E,G) -> b0,b2,b4,b3,b7
    0xD5, // 3 (A,B,C,D,G) -> b0,b2,b6,b4,b7
    0xC6, // 4 (B,C,F,G)   -> b2,b6,b1,b7
    0xD3, // 5 (A,C,D,F,G) -> b0,b6,b4,b1,b7
    0xDB, // 6 (A,C,D,E,F,G) -> b0,b6,b4,b3,b1,b7
    0x45, // 7 (A,B,C)     -> b0,b2,b6
    0xDF, // 8 (全部)      -> b0,b1,b2,b3,b4,b6,b7
    0xD7, // 9 (A,B,C,D,F,G) -> b0,b2,b6,b4,b1,b7
    0x00, // Blank (全灭)
    0x80  // - (只亮G)
};

static uint8_t _brightness = TM1637_BRIGHTNESS_DEF; // 默认亮度

// ==========================================
//  2. 底层通讯协议 (I2C-Like)
// ==========================================
void TM1637_Start(void)
{
    CLK_HIGH();
    DIO_HIGH();
    TM1637_Delay();
    DIO_LOW();
    TM1637_Delay();
    CLK_LOW();
}

void TM1637_Stop(void)
{
    CLK_LOW();
    TM1637_Delay();
    DIO_LOW();
    TM1637_Delay();
    CLK_HIGH();
    TM1637_Delay();
    DIO_HIGH();
}

void TM1637_WriteByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        CLK_LOW();
        if (data & 0x01) DIO_HIGH();
        else DIO_LOW();
        TM1637_Delay();
        CLK_HIGH();
        TM1637_Delay();
        data >>= 1;
    }

    // ACK 信号 (TM1637 会拉低 DIO，我们只要产生时钟即可，不一定要读)
    CLK_LOW();
    DIO_HIGH(); // 释放 DIO 供从机拉低
    TM1637_Delay();
    CLK_HIGH();
    TM1637_Delay();
    CLK_LOW();
}

// ==========================================
//  3. 高层功能
// ==========================================

void TM1637_Init(void)
{
    // GPIO 已经在 Board_Init 里初始化过了，这里只要确保电平状态
    CLK_HIGH();
    DIO_HIGH();
    TM1637_SetBrightness(_brightness);
}

void TM1637_SetBrightness(uint8_t brightness)
{
    _brightness = brightness & 0x07;
}

// 核心函数：把两个温度值写到 6 个乱序的数码管上
void TM1637_Update(int iron_temp, int gun_temp)
{
    uint8_t digits[6]; // 对应 0xC0 ~ 0xC5 的显示内容

    // --- 1. 数据拆分与映射 ---
    // 根据你的图片 image_4c532e.png 逆向的地址映射：

    // *** 烙铁 (Display 1) ***
    // 你的接线：
    // 百位 (Pin12) -> GRID3 (地址 0xC2)
    // 十位 (Pin9)  -> GRID2 (地址 0xC1)
    // 个位 (Pin8)  -> GRID1 (地址 0xC0)
    
    // 处理烙铁数值
    if (iron_temp > 999) iron_temp = 999;
    digits[2] = SegmentMap[iron_temp / 100];       // GRID3 (0xC2) -> 百位
    digits[1] = SegmentMap[(iron_temp / 10) % 10]; // GRID2 (0xC1) -> 十位
    digits[0] = SegmentMap[iron_temp % 10];        // GRID1 (0xC0) -> 个位

    // *** 风枪 (Display 2) ***
    // 你的接线：
    // 百位 (Pin12) -> GRID6 (地址 0xC5)
    // 十位 (Pin9)  -> GRID4 (地址 0xC3) <-- 注意！这里跳过了 GRID5
    // 个位 (Pin8)  -> GRID5 (地址 0xC4) <-- 注意！这里 GRID5 是个位
    
    // 处理风枪数值
    if (gun_temp > 999) gun_temp = 999;
    digits[5] = SegmentMap[gun_temp / 100];       // GRID6 (0xC5) -> 百位
    digits[3] = SegmentMap[(gun_temp / 10) % 10]; // GRID4 (0xC3) -> 十位
    digits[4] = SegmentMap[gun_temp % 10];        // GRID5 (0xC4) -> 个位

    // --- 2. 发送数据 ---
    TM1637_Start();
    TM1637_WriteByte(0x40); // 自动地址增加模式
    TM1637_Stop();

    TM1637_Start();
    TM1637_WriteByte(0xC0); // 起始地址
    
    // 依次写入 0xC0 ~ 0xC5
    for(int i=0; i<6; i++) {
        TM1637_WriteByte(digits[i]);
    }
    TM1637_Stop();

    // --- 3. 开显示 (控制亮度) ---
    TM1637_Start();
    TM1637_WriteByte(0x88 | _brightness); // 开显示 + 亮度
    TM1637_Stop();
}

// 辅助函数：读取一个字节
static uint8_t TM1637_ReadByte(void)
{
    uint8_t data = 0;

    // 1. 切换 DIO 为输入模式
    TM1637_DIO_IN();

    for (uint8_t i = 0; i < 8; i++)
    {
        CLK_LOW();
        // TM1637 在时钟下降沿准备数据，我们在低电平期间不需要动
        TM1637_Delay();

        CLK_HIGH(); // 上升沿读取数据
        TM1637_Delay();

        data >>= 1;
        if (TM1637_DIO_READ()) {
            data |= 0x80; // LSB First
        }
    }

    // 2. 读取结束，发送 ACK (其实是等待 ACK)
    CLK_LOW();
    TM1637_DIO_OUT(); // 恢复为输出
    DIO_LOW();        // 拉低准备产生 ACK
    TM1637_Delay();
    CLK_HIGH();
    TM1637_Delay();
    CLK_LOW();

    return data;
}

// ==========================================
//  新增：读取按键键值
//  返回值: 0xFF=无按键, 其他值=按键码
// ==========================================
uint8_t TM1637_ReadKeys(void)
{
    uint8_t key_code;

    TM1637_Start();
    TM1637_WriteByte(0x42); // 发送读按键命令
    key_code = TM1637_ReadByte();
    TM1637_Stop();

    // 0xFF 通常表示无按键 (具体要看你的板子，有的板子无按键是 0x00)
    // 我们可以取反一下，让逻辑更直观
    // 通常 TM1637 无按键返回 0xF7 或者其他值，我们最好先打印出来看看
    return key_code;
}

void TM1637_Test(void)
{
    TM1637_Update(888, 888); // 满屏显示 888 888
}
