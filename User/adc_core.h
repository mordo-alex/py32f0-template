#ifndef __ADC_CORE_H
#define __ADC_CORE_H

#include "py32f0xx_hal.h"

// ==========================================
// ★ 硬件引脚映射
// ==========================================
#define IRON_ADC_PORT       GPIOA
#define IRON_ADC_PIN        GPIO_PIN_2
#define IRON_ADC_CH         ADC_CHANNEL_2

#define GUN_ADC_PORT        GPIOA
#define GUN_ADC_PIN         GPIO_PIN_3
#define GUN_ADC_CH          ADC_CHANNEL_3

// 全局唯一的 ADC 句柄
extern ADC_HandleTypeDef hadc_core;

// ==========================================
// ★ 暴漏的 API 接口
// ==========================================
void ADC_Core_Init(void);
uint16_t ADC_Read_Iron_Strict(void);
uint16_t ADC_Read_Gun_Lazy(void);

// ★ 修复报错 1：高级工厂模式底噪校准
void ADC_Factory_Calibrate_Zero(void);

#endif
