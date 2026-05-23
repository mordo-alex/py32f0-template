#ifndef __TM1637_H
#define __TM1637_H

#include "py32f0xx_hal.h"

typedef enum {
    UI_MODE_OFF = 0,    // 关机熄屏
    UI_MODE_NORMAL,     // 正常显示数字
    UI_MODE_ERROR,      // 报错显示 (---)
    UI_MODE_MENU        // 菜单显示 (F-x)
} UI_Mode_t;

// ★ 纯净的数据结构，不掺杂任何业务变量
typedef struct {
    UI_Mode_t iron_mode;
    int iron_val;       
    uint8_t iron_anim;  // 0:常亮, 1:跑马灯

    UI_Mode_t gun_mode;
    int gun_val;        
    uint8_t gun_anim;   
} UI_DisplayData_t;

void TM1637_Init(void);
uint8_t TM1637_ReadKeys(void); 
void TM1637_UpdateDisplay(UI_DisplayData_t *data); // ★ 唯一的渲染入口

#endif
