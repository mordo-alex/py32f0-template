#include "py32f0xx_bsp_printf.h"
#include "board_config.h"
#include "tm1637.h"
#include "py32f0xx_hal.h"

int main(void)
{
    // 1. 底层外设初始化
    HAL_Init();
    Board_Init();
    TM1637_Init();
    BSP_USART_Config();

    // 2. 打印开机横幅
    printf("\r\n");
    printf("=================================\r\n");
    printf("  Dual Station: T12 + 858 V2.0   \r\n");
    printf("  UI Engine Boot UP... OK!       \r\n");
    printf("=================================\r\n");

    // 3. 终极主循环
    while (1)
    {
        // 把所有的 UI 交互、按键防抖、数码管刷新全部交托给 TM1637 模块
        TM1637_ProcessUI();

        // 给主循环加一点延时。50ms 是人类按键最舒服的防抖间隔
        // 以后加了 PID 运算，也不影响这 50ms 的 UI 刷新率
        HAL_Delay(50); 
    }
}

void APP_ErrorHandler(void) { while (1); }
