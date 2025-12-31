#ifndef __SETTINGS_H
#define __SETTINGS_H

#include "py32f0xx_hal.h"

// Flash 存储地址 (PY32F030F18P6 是 64KB Flash)
// 我们选倒数第 2 页，防止跟程序代码冲突，也留点余量
// 64KB = 0x10000 字节。起始 0x0800 0000。
// 最后一页通常在 0x0800 Fxxx 附近。我们选 0x0800 F000 绝对安全。
#define FLASH_USER_START_ADDR   0x0800F000 

// 数据结构
typedef struct {
    uint16_t iron_target; // 烙铁设定温度
    uint16_t gun_target;  // 风枪设定温度
    uint16_t magic_num;   // 这是一个标记，用来判断Flash是不是第一次用 (是不是空的)
} SystemSettings_t;

// 标记值 (随便写个特殊的数)
#define SETTINGS_MAGIC 0x5AA5

// 全局变量声明
extern SystemSettings_t sys_settings;

// 函数声明
void Settings_Load(void);
void Settings_Save(void);

#endif
