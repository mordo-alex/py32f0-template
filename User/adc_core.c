#include "adc_core.h"
#include <stdlib.h>

// 宏定义：允许的最大突变刻度（超过判定为尖峰脉冲）
#define ADC_MAX_JUMP        300  

ADC_HandleTypeDef hadc_core;

static uint32_t iron_filter_val = 0;
static uint32_t gun_filter_val = 0;
static uint16_t iron_zero_offset = 0; // 存储开机捕获的运放底噪

/**
  * @brief 初始化全局共用的 ADC1 硬件
  */
void ADC_Core_Init(void) {
    __HAL_RCC_ADC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = IRON_ADC_PIN | GUN_ADC_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    hadc_core.Instance = ADC1;
    hadc_core.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc_core.Init.Resolution = ADC_RESOLUTION_12B; 
    hadc_core.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc_core.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
    hadc_core.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc_core.Init.LowPowerAutoWait = DISABLE;
    hadc_core.Init.ContinuousConvMode = DISABLE;
    hadc_core.Init.DiscontinuousConvMode = DISABLE;
    hadc_core.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    HAL_ADC_Init(&hadc_core);

    HAL_ADCEx_Calibration_Start(&hadc_core);
}

/**
  * @brief 捕获运放的冷态零点漂移底噪（上电绝未加热时调用一次）
  */
void ADC_Capture_Zero_Offset(void) {
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t sum = 0;

    HAL_ADC_Stop(&hadc_core);
    sConfig.Channel = IRON_ADC_CH;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc_core, &sConfig);

    // 连续抓取 10 次求平均
    for(int i = 0; i < 10; i++) {
        HAL_ADC_Start(&hadc_core);
        HAL_ADC_PollForConversion(&hadc_core, 5);
        sum += HAL_ADC_GetValue(&hadc_core);
    }
    iron_zero_offset = sum / 10;
}

/**
  * @brief 烙铁霸道读取：最高优先级、突发采样、脉冲拦截、底噪剔除
  */
uint16_t ADC_Read_Iron_Strict(void) {
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t sum = 0;

    // 1. 强行终止风枪可能正在进行的转换，让路给 T12 的黄金断电窗
    HAL_ADC_Stop(&hadc_core);

    sConfig.Channel = IRON_ADC_CH;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5; 
    HAL_ADC_ConfigChannel(&hadc_core, &sConfig);

    // 2. 第一重滤波：突发过采样 (连读4次)
    for(int i = 0; i < 4; i++) {
        HAL_ADC_Start(&hadc_core);
        HAL_ADC_PollForConversion(&hadc_core, 5);
        sum += HAL_ADC_GetValue(&hadc_core);
    }
    uint32_t raw_avg = sum >> 2; 

    // 3. 硬件断线拦截：如果已经开路/没插手柄，直接返回满量程，不参与滤波
    if (raw_avg >= 4000) {
        return 4095;
    }

    // 4. 技术进阶：剔除 LM358 运放失调电压底噪
    if (raw_avg > iron_zero_offset) {
        raw_avg = raw_avg - iron_zero_offset;
    } else {
        raw_avg = 0;
    }

    // 5. 技术进阶：恶性突变脉冲拦截 (Outlier Rejection)
    if (iron_filter_val != 0) {
        uint32_t current_ema = iron_filter_val >> 2;
        if (abs((int)raw_avg - (int)current_ema) > ADC_MAX_JUMP) {
            raw_avg = current_ema; // 飞来毛刺，丢弃新值，沿用上次的值
        }
    }

    // 6. 第二重滤波：EMA 低通滤波 (新值 25%, 历史 75%)
    if (iron_filter_val == 0) { iron_filter_val = raw_avg << 2; }
    iron_filter_val = iron_filter_val - (iron_filter_val >> 2) + raw_avg;

    return (uint16_t)(iron_filter_val >> 2);
}

/**
  * @brief 风枪温柔读取：后台随缘、排队等待
  */
uint16_t ADC_Read_Gun_Lazy(void) {
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t sum = 0;
    uint8_t success_count = 0;

    HAL_ADC_Stop(&hadc_core); 

    sConfig.Channel = GUN_ADC_CH;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5; 
    HAL_ADC_ConfigChannel(&hadc_core, &sConfig);

    for(int i = 0; i < 4; i++) {
        HAL_ADC_Start(&hadc_core);
        if (HAL_ADC_PollForConversion(&hadc_core, 2) == HAL_OK) {
            sum += HAL_ADC_GetValue(&hadc_core);
            success_count++;
        }
    }

    if (success_count > 0) {
        uint32_t raw_avg = sum / success_count;
        if (gun_filter_val == 0) { gun_filter_val = raw_avg << 3; } // 风枪滤得更黏稠
        gun_filter_val = gun_filter_val - (gun_filter_val >> 3) + raw_avg;
    }

    return (uint16_t)(gun_filter_val >> 3);
}
