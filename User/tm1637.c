#include "tm1637.h"
#include "board_config.h"

#define CLK_LOW()   HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_RESET)
#define CLK_HIGH()  HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_SET)
#define DIO_LOW()   HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_RESET)
#define DIO_HIGH()  HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_SET)
#define DIO_READ()  HAL_GPIO_ReadPin(TM1637_DIO_PORT, TM1637_DIO_PIN)

static void TM1637_Delay(void)
{
    for (volatile int i = 0; i < 50; i++)
    {
        __NOP();
    }
}

static const uint8_t SegmentMap[] = {
    0x5F, 0x44, 0x9D, 0xD5, 0xC6, 0xD3, 0xDB, 0x45, 0xDF, 0xD7, 0x00, 0x40
};

static uint8_t _brightness = 2;

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
        if (data & 0x01)
        {
            DIO_HIGH();
        }
        else
        {
            DIO_LOW();
        }
        TM1637_Delay();
        CLK_HIGH();
        TM1637_Delay();
        data >>= 1;
    }
    
    CLK_LOW();
    DIO_HIGH();
    TM1637_Delay();
    CLK_HIGH();
    TM1637_Delay();
    CLK_LOW();
}

uint8_t TM1637_ReadKeys(void)
{
    uint8_t key = 0;
    
    TM1637_Start();
    TM1637_WriteByte(0x42);
    TM1637_DIO_IN();
    TM1637_Delay();

    for (int i = 0; i < 8; i++)
    {
        CLK_LOW();
        TM1637_Delay();
        key >>= 1;
        CLK_HIGH();
        TM1637_Delay();
        
        if (DIO_READ() == GPIO_PIN_SET)
        {
            key |= 0x80;
        }
    }

    TM1637_DIO_OUT();
    CLK_LOW();
    TM1637_Delay();
    DIO_LOW();
    TM1637_Delay();
    CLK_HIGH();
    TM1637_Delay();
    DIO_HIGH();
    
    return key;
}

void TM1637_WriteRaw(uint8_t *buff)
{
    TM1637_Start();
    TM1637_WriteByte(0x40);
    TM1637_Stop();

    TM1637_Start();
    TM1637_WriteByte(0xC0);
    for (int i = 0; i < 6; i++)
    {
        TM1637_WriteByte(buff[i]);
    }
    TM1637_Stop();

    TM1637_Start();
    TM1637_WriteByte(0x88 | _brightness);
    TM1637_Stop();
}

void TM1637_Init(void)
{
    CLK_HIGH();
    DIO_HIGH();
}

void TM1637_UpdateDisplay(UI_DisplayData_t *data)
{
    uint8_t raw_buff[6] = {0};
    static uint8_t dp_frame = 0;
    static uint8_t anim_divider = 0;
    static uint8_t blink_tick = 0;

    anim_divider++;
    if (anim_divider >= 2)
    {
        anim_divider = 0;
        dp_frame = (dp_frame + 1) % 3;
        blink_tick = (blink_tick + 1) % 4;
    }

    // ==========================================
    // 左侧烙铁渲染
    // ==========================================
    if (data->iron_mode == UI_MODE_OFF)
    {
        raw_buff[2] = 0;
        raw_buff[1] = 0;
        raw_buff[0] = 0;
    }
    else if (data->iron_mode == UI_MODE_ERROR)
    {
        if (blink_tick < 2)
        {
            raw_buff[2] = 0x80;
            raw_buff[1] = 0x80;
            raw_buff[0] = 0x80;
        }
    }
    else if (data->iron_mode == UI_MODE_MENU)
    {
        raw_buff[2] = 0x8B;
        raw_buff[1] = 0x80;
        raw_buff[0] = SegmentMap[data->iron_val % 10];
        
        if (data->iron_anim == 1 && blink_tick < 2)
        {
            raw_buff[2] |= 0x20;
            raw_buff[1] |= 0x20;
            raw_buff[0] |= 0x20;
        }
    }
    else
    {
        raw_buff[2] = SegmentMap[(data->iron_val / 100) % 10];
        raw_buff[1] = SegmentMap[(data->iron_val / 10) % 10];
        raw_buff[0] = SegmentMap[data->iron_val % 10];

        if (data->iron_anim == 1)
        {
            if (dp_frame == 0)
            {
                raw_buff[2] |= 0x20;
            }
            else if (dp_frame == 1)
            {
                raw_buff[1] |= 0x20;
            }
            else if (dp_frame == 2)
            {
                raw_buff[0] |= 0x20;
            }
        }
        else
        {
            raw_buff[0] |= 0x20;
        }
    }

    // ==========================================
    // 右侧风枪渲染
    // ==========================================
    if (data->gun_mode == UI_MODE_OFF)
    {
        raw_buff[5] = 0;
        raw_buff[3] = 0;
        raw_buff[4] = 0;
    }
    else if (data->gun_mode == UI_MODE_ERROR)
    {
        if (blink_tick < 2)
        {
            raw_buff[5] = 0x80;
            raw_buff[3] = 0x80;
            raw_buff[4] = 0x80;
        }
    }
    else
    {
        raw_buff[5] = SegmentMap[(data->gun_val / 100) % 10];
        raw_buff[3] = SegmentMap[(data->gun_val / 10) % 10];
        raw_buff[4] = SegmentMap[data->gun_val % 10];

        if (data->gun_anim == 1)
        {
            if (dp_frame == 0)
            {
                raw_buff[5] |= 0x20;
            }
            else if (dp_frame == 1)
            {
                raw_buff[3] |= 0x20;
            }
            else if (dp_frame == 2)
            {
                raw_buff[4] |= 0x20;
            }
        }
        else
        {
            raw_buff[4] |= 0x20;
        }
    }

    TM1637_WriteRaw(raw_buff);
}
