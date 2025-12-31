#ifndef __TM1637_H
#define __TM1637_H

#include "py32f0xx_hal.h"

// 亮度设置 (0-7)
#define TM1637_BRIGHTNESS_MIN   0
#define TM1637_BRIGHTNESS_MAX   7
#define TM1637_BRIGHTNESS_DEF   2

// 初始化
void TM1637_Init(void);

// 设置亮度 (0~7)
void TM1637_SetBrightness(uint8_t brightness);

// 核心功能：刷新显示
// iron_temp: 烙铁温度 (0-999)
// gun_temp:  风枪温度 (0-999)
void TM1637_Update(int iron_temp, int gun_temp);

uint8_t TM1637_ReadKeys(void);

// 测试用：检查是否所有段都亮
void TM1637_Test(void);

#endif
