#include "tm1637.h"
#include "board_config.h"

// ==========================================
//  底层 GPIO
// ==========================================
#define CLK_LOW()   HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_RESET)
#define CLK_HIGH()  HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_SET)
#define DIO_LOW()   HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_RESET)
#define DIO_HIGH()  HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_SET)

static void TM1637_Delay(void) { for(volatile int i = 0; i < 50; i++) { __NOP(); } }

// ==========================================
//  1. 段码表 (Bit 5 = 小数点)
// ==========================================
static const uint8_t SegmentMap[] = {
    0x5F, 0x44, 0x9D, 0xD5, 0xC6, 0xD3, 0xDB, 0x45, 0xDF, 0xD7, 0x00, 0x40
};
static uint8_t _brightness = 2;

// ==========================================
//  2. 底层通讯
// ==========================================
void TM1637_Start(void) {
    CLK_HIGH(); DIO_HIGH(); TM1637_Delay();
    DIO_LOW(); TM1637_Delay();
    CLK_LOW();
}
void TM1637_Stop(void) {
    CLK_LOW(); TM1637_Delay();
    DIO_LOW(); TM1637_Delay();
    CLK_HIGH(); TM1637_Delay();
    DIO_HIGH();
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
void TM1637_Init(void) { CLK_HIGH(); DIO_HIGH(); }
void TM1637_SetBrightness(uint8_t brightness) { _brightness = brightness & 0x07; }

// ==========================================
//  3. 核心功能: 最终修正版
// ==========================================
void TM1637_Update(int iron_temp, int gun_temp)
{
    uint8_t raw_buff[6]; // 物理显存 0xC0~0xC5

    if(iron_temp > 999) iron_temp = 999;
    if(gun_temp > 999)  gun_temp = 999;

    uint8_t iron_100 = SegmentMap[iron_temp / 100];
    uint8_t iron_10  = SegmentMap[(iron_temp / 10) % 10];
    uint8_t iron_1   = SegmentMap[iron_temp % 10];

    uint8_t gun_100  = SegmentMap[gun_temp / 100];
    uint8_t gun_10   = SegmentMap[(gun_temp / 10) % 10];
    uint8_t gun_1    = SegmentMap[gun_temp % 10];

    // --- 映射修正 (Swap Middle & Right) ---
    
    // 右侧 (风枪):
    // 右1 (百位) -> 0xC2 (保持不变)
    raw_buff[2] = gun_100;
    // 右2 (十位) -> 改为 0xC1 (原先是 C0)
    raw_buff[1] = gun_10; 
    // 右3 (个位) -> 改为 0xC0 (原先是 C1)
    raw_buff[0] = gun_1;

    // 左侧 (烙铁):
    // 左1 (百位) -> 0xC5 (保持不变)
    raw_buff[5] = iron_100;
    // 左2 (十位) -> 改为 0xC3 (原先是 C4)
    raw_buff[3] = iron_10;
    // 左3 (个位) -> 改为 0xC4 (原先是 C3)
    raw_buff[4] = iron_1;

    // --- 发送 ---
    TM1637_WriteRaw(raw_buff);
}

void TM1637_WriteRaw(uint8_t *buff) {
    TM1637_Start(); TM1637_WriteByte(0x40); TM1637_Stop();
    TM1637_Start(); TM1637_WriteByte(0xC0); 
    for(int i=0; i<6; i++) TM1637_WriteByte(buff[i]);
    TM1637_Stop();
    TM1637_Start(); TM1637_WriteByte(0x88 | _brightness); TM1637_Stop();
}
uint8_t TM1637_ReadKeys(void) { return 0xFF; }
