#include "settings.h"
#include "py32f0xx_bsp_printf.h"
#include <string.h> // 需要用到 memset

SystemSettings_t sys_settings;

// 读取设置 (保持不变，可以直接读内存)
void Settings_Load(void)
{
    uint32_t addr = FLASH_USER_START_ADDR;
    
    // 读取偏移 4 字节处的 Magic Number
    uint16_t stored_magic = *(__IO uint16_t*)(addr + 4);

    // 如果 Magic 不对，说明是新芯片或数据为空
    if (stored_magic != SETTINGS_MAGIC) {
        printf("Flash Empty! Using Defaults.\r\n");
        // 加载默认值
        sys_settings.iron_target = 300;
        sys_settings.gun_target  = 350;
        sys_settings.magic_num   = SETTINGS_MAGIC;
        
        // 第一次顺便保存一下
        Settings_Save();
    } else {
        printf("Settings Loaded.\r\n");
        // 直接读取地址
        sys_settings.iron_target = *(__IO uint16_t*)(addr);
        sys_settings.gun_target  = *(__IO uint16_t*)(addr + 2);
    }
}

// 保存设置 (核心修改：改为页编程)
void Settings_Save(void)
{
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t PAGEError = 0;
    
    // PY32F030 的一页通常是 128 字节
    // 我们准备一个 32 个 uint32_t 的缓存数组 (32 * 4 = 128 bytes)
    uint32_t flash_buffer[32];

    // 1. 初始化缓冲区为 0xFF (擦除后的状态)
    // 这样没用到的地方保持空白，不会乱写
    for(int i=0; i<32; i++) {
        flash_buffer[i] = 0xFFFFFFFF;
    }

    // 2. 将数据“打包”进缓冲区
    // 我们的读取逻辑是：
    // addr + 0 : iron (16bit)
    // addr + 2 : gun  (16bit)
    // addr + 4 : magic (16bit)
    
    // Word 0 (低16位存iron, 高16位存gun)
    flash_buffer[0] = (uint32_t)sys_settings.iron_target | ((uint32_t)sys_settings.gun_target << 16);
    
    // Word 1 (低16位存magic)
    flash_buffer[1] = (uint32_t)SETTINGS_MAGIC;

    // 3. 解锁 Flash
    HAL_FLASH_Unlock();

    // 4. 擦除页
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGEERASE;
    EraseInitStruct.PageAddress = FLASH_USER_START_ADDR;
    EraseInitStruct.NbPages     = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError) != HAL_OK) {
        printf("Flash Erase Failed!\r\n");
        HAL_FLASH_Lock();
        return;
    }

    // 5. 写入一整页 (Page Program)
    // 注意参数：FLASH_TYPEPROGRAM_PAGE, 地址, 缓冲区指针
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, FLASH_USER_START_ADDR, flash_buffer) != HAL_OK) {
        printf("Flash Write Failed!\r\n");
    } else {
        printf("Settings Saved: Iron=%d, Gun=%d\r\n", sys_settings.iron_target, sys_settings.gun_target);
    }

    // 6. 上锁
    HAL_FLASH_Lock();
}
