#include "board_config.h"
#include "py32f0xx_bsp_printf.h"

// 全局句柄
TIM_HandleTypeDef htim3; // 用于烙铁 PWM
ADC_HandleTypeDef hadc;  // 用于测温

// ============================================================
//  1. 系统时钟配置 (System Clock Configuration)
//  配置为 HSI 24MHz，这是 PY32 的经典速度
// ============================================================
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  // 配置 HSI (内部高速时钟)
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1; // 不分频，跑满 24MHz
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_24MHz;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    while(1); // 初始化失败死循环
  }

  // 配置总线时钟
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI; // 系统时钟源选 HSI
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    while(1);
  }
}

// ============================================================
//  2. GPIO 初始化 (开关输入、控制输出、屏幕)
// ============================================================
static void GPIO_Init_All(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // 1. 开启 GPIO 时钟
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  // ---------------------------------------------------------
  // [安全第一] 先设置输出引脚的初始电平，防止上电瞬间误动作
  // ---------------------------------------------------------
  
  // 烙铁 PWM (PB5): 暂时拉低，稍后由 TIM3 接管
  HAL_GPIO_WritePin(IRON_HEATER_PORT, IRON_HEATER_PIN, GPIO_PIN_RESET); 
  
  // 风枪加热 (PB7): 拉高 (假设光耦低电平触发，高电平为关)
  HAL_GPIO_WritePin(GUN_HEATER_PORT, GUN_HEATER_PIN, GPIO_PIN_SET); 
  
  // 风扇 (PA9): 拉低 (关)
  HAL_GPIO_WritePin(GUN_FAN_PORT, GUN_FAN_PIN, GPIO_PIN_RESET);

  // 屏幕 (PA10, PA11): 拉高 (空闲)
  HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_SET);

  // ---------------------------------------------------------
  // [输出] 配置控制引脚 (推挽输出)
  // ---------------------------------------------------------
  // 风枪加热 (PB7)
  GPIO_InitStruct.Pin = GUN_HEATER_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GUN_HEATER_PORT, &GPIO_InitStruct);

  // 风扇 (PA9)
  GPIO_InitStruct.Pin = GUN_FAN_PIN;
  HAL_GPIO_Init(GUN_FAN_PORT, &GPIO_InitStruct);

  // 屏幕 CLK/DIO (PA10, PA11)
  GPIO_InitStruct.Pin = TM1637_CLK_PIN | TM1637_DIO_PIN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // ---------------------------------------------------------
  // [输入] 配置开关引脚 (上拉输入，因为开关对地短路)
  // ---------------------------------------------------------
  // PB1 (磁控), PB2 (风枪开关)
  GPIO_InitStruct.Pin = GUN_REED_PIN | GUN_SW_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP; // 必须上拉
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // PA4 (烙铁开关)
  GPIO_InitStruct.Pin = IRON_SW_PIN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// ============================================================
//  3. PWM 初始化 (TIM3 控制 PB5 烙铁)
//  目标: 1kHz 频率
// ============================================================
static void PWM_TIM3_Init(void)
{
  //TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  //TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // 1. 开启 TIM3 时钟
  __HAL_RCC_TIM3_CLK_ENABLE();

  // 2. 配置 PB5 为复用功能 (AF_PP)，连接到 TIM3_CH2
  GPIO_InitStruct.Pin = IRON_HEATER_PIN; // PB5
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; 
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM3; // 查手册 PB5 复用功能是 AF1
  HAL_GPIO_Init(IRON_HEATER_PORT, &GPIO_InitStruct);

  // 3. 配置定时器基础参数
  // 主频 24MHz。
  // 我们想要 1kHz PWM -> 周期 1ms -> 1000us
  // 设 Prescaler = 24-1 => 计数器频率 1MHz (1us 一跳)
  // 设 Period = 1000-1 => 1000跳 溢出一次 = 1ms
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 24 - 1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1000 - 1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    while(1);
  }

  // 4. 配置 PWM 模式
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    while(1);
  }

  // 5. 配置通道 2 (Channel 2)
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0; // 初始占空比 0 (关)
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH; // 高电平有效 (根据MOS管驱动电路定)
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    while(1);
  }

  // 6. 启动 PWM 输出
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
}

// ============================================================
//  4. ADC 初始化 (用于 PA2, PA3 测温)
// ============================================================
static void ADC_Init(void)
{
  //ADC_ChannelConfTypeDef sConfig = {0};
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // 1. 开启 ADC 时钟
  __HAL_RCC_ADC_CLK_ENABLE();

  // 2. 配置 PA2, PA3 为模拟输入模式 (关键!)
  GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // 3. 配置 ADC 参数
  hadc.Instance = ADC1;
  hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4; // 时钟分频
  hadc.Init.Resolution = ADC_RESOLUTION_12B;           // 12位精度 (0-4095)
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD; // 扫描模式
  hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc.Init.LowPowerAutoWait = ENABLE;                 // 低功耗等待
  hadc.Init.ContinuousConvMode = DISABLE;              // 单次转换 (我们需要手动触发)
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;     // 软件触发
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.DMAContinuousRequests = DISABLE;
  hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  
  if (HAL_ADC_Init(&hadc) != HAL_OK)
  {
    while(1);
  }

  // 校准 ADC (PY32 必须做的)
  HAL_ADCEx_Calibration_Start(&hadc);
}

// ============================================================
//  辅助函数: 读取指定通道的 ADC 值
//  channel 参数: ADC_CHANNEL_2 或 ADC_CHANNEL_3
// ============================================================
uint16_t Board_ADC_Read(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    // 配置要读取的通道
    sConfig.Channel = channel;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5; // 采样时间越长越稳
    
    if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
    {
        return 0;
    }

    // 开始转换
    HAL_ADC_Start(&hadc);
    
    // 等待转换完成
    HAL_ADC_PollForConversion(&hadc, 10); // 等待 10ms
    
    // 获取结果
    uint16_t val = HAL_ADC_GetValue(&hadc);
    
    // 停止 (省电)
    HAL_ADC_Stop(&hadc);
    
    return val;
}

// ============================================================
//  辅助函数: 设置烙铁 PWM 占空比
//  duty: 0 ~ 1000 (对应 0% ~ 100%)
// ============================================================
void Board_Iron_SetPWM(uint16_t duty)
{
    if(duty > 1000) duty = 1000;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, duty);
}

// ============================================================
//  总初始化函数 (在 main 中调用这个即可)
// ============================================================
void Board_Init(void)
{
    SystemClock_Config(); // 1. 先搞定 24MHz 时钟
    GPIO_Init_All();      // 2. 搞定所有 IO
    PWM_TIM3_Init();      // 3. 启动烙铁 PWM
    ADC_Init();           // 4. 启动眼睛 (ADC)
    
    BSP_USART_Config();   // 5. 串口 (用于 printf 调试)
    printf("Board Init Success!\r\n");
}
