#include "py32f0xx_bsp_printf.h"
#include "board_config.h"
#include "tm1637.h"
#include "py32f0xx_hal.h"

const uint8_t SEG_MAP[] = {
    0x5F, 0x44, 0x9D, 0xD5, 0xC6, 0xD3, 0xDB, 0x45, 0xDF, 0xD7
};

// ★★★ 修正后的视觉顺序 ★★★
// 期望效果: [左1] [左2] [左3] [右1] [右2] [右3]
// 物理地址: [0xC5][0xC3][0xC4][0xC2][0xC1][0xC0]
// 对应下标:   5     3     4     2     1     0
const uint8_t VISUAL_ORDER[6] = {5, 3, 4, 2, 1, 0};

void TM1637_Custom_Test(void)
{
    static int left_num = 0;   
    static int right_num = 9;  
    static int dot_step = 0;   
    
    uint8_t buff[6] = {0};

    // 填充数字
    buff[5] = SEG_MAP[left_num]; // 左1
    buff[3] = SEG_MAP[left_num]; // 左2 (改)
    buff[4] = SEG_MAP[left_num]; // 左3 (改)

    buff[2] = SEG_MAP[right_num]; // 右1
    buff[1] = SEG_MAP[right_num]; // 右2 (改)
    buff[0] = SEG_MAP[right_num]; // 右3 (改)

    // 小数点 (0x20)
    uint8_t target_index = VISUAL_ORDER[dot_step];
    buff[target_index] |= 0x20; 

    TM1637_WriteRaw(buff);
    
    left_num++; if(left_num > 9) left_num = 0;
    right_num--; if(right_num < 0) right_num = 9;
    dot_step++; if(dot_step > 5) dot_step = 0;
}

int main(void)
{
    HAL_Init(); 
    Board_Init();
    TM1637_Init();

    while (1)
    {
        TM1637_Custom_Test();
        HAL_Delay(500); 
    }
}
void APP_ErrorHandler(void) { while (1); }
