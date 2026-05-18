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
    // 1. 底层与时钟初始化
    HAL_Init();
    Board_Init();   // 假设你的 GPIO、ADC 初始化都在这里面
    TM1637_Init();

    // 2. ★★★ 极其关键：初始化调试串口 ★★★
    // 只有调用了这个，py32f0xx_bsp_printf.c 里的重定向才会生效！
    BSP_USART_Config();

    // 3. 打印开机横幅 (测试发送)
    printf("\r\n");
    printf("=================================\r\n");
    printf("  T12 Soldering Station V1.0 \r\n");
    printf("  System Boot UP... OK!      \r\n");
    printf("=================================\r\n");

    while (1)
    {
        // 跑你的数码管动画
        TM1637_Custom_Test();

        // ---------------------------------------------------------
        // ★ 核心测试点：通过串口实时监控系统状态
        // ---------------------------------------------------------

        // 【测试 1】: 纯心跳包（证明串口没卡死）
        printf("[Sys] Heartbeat ticking...\r\n");
        // 假设你的串口句柄叫 DebugUartHandle。如果是 huart1，就把第一个参数换成 &huart1
        HAL_UART_Transmit(&DebugUartHandle, (uint8_t *)"ALIVE!\r\n", 8, 1000);
        // 【测试 2】: 读 ADC（等你配置好 ADC 后把下面的注释打开）
        /*
        uint32_t t12_adc_raw = HAL_ADC_GetValue(&hadc); // 假设你用的标准 HAL 轮询读取
        printf("[ADC] T12 Raw Temp Value: %lu\r\n", t12_adc_raw);
        */

        HAL_Delay(500); // 500ms 刷新一次，方便串口肉眼看
    }
}

void APP_ErrorHandler(void) { while (1); }
