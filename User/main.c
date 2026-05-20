#include "py32f0xx_bsp_printf.h"
#include "board_config.h"
#include "tm1637.h"
#include "py32f0xx_hal.h"
#include "adc_core.h"
#include "iron_logic.h"
#include "gun_logic.h"
#include "advanced_tools.h"

int iron_target_temp = 300;
int gun_target_temp = 200;

int main(void)
{
    HAL_Init();
    Board_Init();
    BSP_USART_Config();

    printf("\r\n=================================\r\n");
    printf("  Dual Station: T12 + 858 V2.0   \r\n");
    printf("  Decoupled Engine Booting...    \r\n");
    
    TM1637_Init();         
    ADC_Core_Init();        
    Iron_Init();            
    Gun_Init();             
    Advanced_Tools_Init();  

    printf("  All Modules Ready!             \r\n");
    printf("=================================\r\n");

    uint32_t debug_tick = 0;

    while (1)
    {
        // 1. UI 交互与屏幕渲染引擎
        TM1637_ProcessUI();

        // 2. 核心状态机护驾
        Iron_Process(iron_target_temp);
        Gun_Process(gun_target_temp);

        // 3. 硬件雷达：每秒打印一次物理开关真实电平
        if (HAL_GetTick() - debug_tick > 1000) {
            debug_tick = HAL_GetTick();
            int iron_sw = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6); 
            int gun_sw  = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8);
            printf("[Radar] Iron SW(PB6): %d | Gun SW(PA8): %d\r\n", iron_sw, gun_sw);
        }

        // 4. 50ms 心跳
        HAL_Delay(50);
    }
}

void APP_ErrorHandler(void) { while (1); }
