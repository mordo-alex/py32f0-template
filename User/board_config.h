#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

#include "py32f0xx_hal.h"

// ==========================================
//  1. 核心 ADC (PA2, PA3)
// ==========================================
#define ADC_CH_IRON_TEMP        ADC_CHANNEL_2   // PA2 (Pin 5)
#define ADC_CH_GUN_TEMP         ADC_CHANNEL_3   // PA3 (Pin 6)

// ==========================================
//  2. 独立开关输入 (内部上拉, 低电平有效)
// ==========================================
// 风枪磁控 (飞线接原 R11) -> PB4
#define GUN_REED_PIN            GPIO_PIN_4
#define GUN_REED_PORT           GPIOB
#define READ_GUN_REED()         HAL_GPIO_ReadPin(GUN_REED_PORT, GUN_REED_PIN)

// 风枪开关 (飞线接原 R12) -> PB6
#define GUN_SW_PIN              GPIO_PIN_6
#define GUN_SW_PORT             GPIOB
#define READ_GUN_SW()           HAL_GPIO_ReadPin(GUN_SW_PORT, GUN_SW_PIN)

// 烙铁开关 (飞线接原 R16) -> PA8
#define IRON_SW_PIN             GPIO_PIN_8
#define IRON_SW_PORT            GPIOA
#define READ_IRON_SW()          HAL_GPIO_ReadPin(IRON_SW_PORT, IRON_SW_PIN)

// ==========================================
//  3. 输出控制
// ==========================================
// 烙铁 PWM -> PB5
#define IRON_HEATER_PIN         GPIO_PIN_5
#define IRON_HEATER_PORT        GPIOB

// 风枪加热 -> PB7
#define GUN_HEATER_PIN          GPIO_PIN_7
#define GUN_HEATER_PORT         GPIOB

// 风扇 -> PA9
#define GUN_FAN_PIN             GPIO_PIN_9
#define GUN_FAN_PORT            GPIOA
#define GUN_FAN_ON()            HAL_GPIO_WritePin(GUN_FAN_PORT, GUN_FAN_PIN, GPIO_PIN_SET)
#define GUN_FAN_OFF()           HAL_GPIO_WritePin(GUN_FAN_PORT, GUN_FAN_PIN, GPIO_PIN_RESET)

// ==========================================
//  4. 显示屏 (PA10, PA11)
// ==========================================
#define TM1637_CLK_PIN          GPIO_PIN_10
#define TM1637_CLK_PORT         GPIOA
#define TM1637_DIO_PIN          GPIO_PIN_11
#define TM1637_DIO_PORT         GPIOA

// ==========================================
//  5. 函数声明 (让 main.c 能找到它们！)
// ==========================================
void Board_Init(void);
uint16_t Board_ADC_Read(uint32_t channel);
void Board_Iron_SetPWM(uint16_t duty);

// ==========================================
//  TM1637 底层方向控制 (读按键必须)
// ==========================================
// 设置 DIO 为输入 (浮空输入)
#define TM1637_DIO_IN()   { \
    GPIO_InitTypeDef GPIO_InitStruct = {0}; \
    GPIO_InitStruct.Pin = TM1637_DIO_PIN; \
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; \
    GPIO_InitStruct.Pull = GPIO_PULLUP; \
    HAL_GPIO_Init(TM1637_DIO_PORT, &GPIO_InitStruct); \
}

// 设置 DIO 为输出 (推挽输出)
#define TM1637_DIO_OUT()  { \
    GPIO_InitTypeDef GPIO_InitStruct = {0}; \
    GPIO_InitStruct.Pin = TM1637_DIO_PIN; \
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; \
    GPIO_InitStruct.Pull = GPIO_NOPULL; \
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; \
    HAL_GPIO_Init(TM1637_DIO_PORT, &GPIO_InitStruct); \
}

// 读取 DIO 电平
#define TM1637_DIO_READ() HAL_GPIO_ReadPin(TM1637_DIO_PORT, TM1637_DIO_PIN)

#endif
