/*
 * main.c — FSGlove v2 STM32H7 采集端主程序
 *
 * 目标：STM32H750IBK6 (UFBGA176)，CPU 480 MHz。
 * 本文件提供可直接编译的最小 HAL 启动：时钟树 480MHz、GPIO、2×SPI、USART1(+TX DMA)，
 * 然后进入 acquisition 采集主循环。引脚分配见 stm32h7xx_hal_msp.c（按 PCB 改）。
 *
 * 注1：H7 的 DMA 不能访问 DTCM，故链接脚本把数据放 AXI SRAM，且只开 ICache、关 DCache。
 * 注2：H750 内部 Flash 仅 128KB；时钟用 HSE，自研板按实际晶振频率改 HSEState/PLLM（见下）。
 */
#include "main.h"
#include "fsglove_config.h"
#include "acquisition.h"

/* ---- 外设句柄 ---- */
SPI_HandleTypeDef  hspi1;     /* 总线0 (FSG_SPI_BUS0) */
SPI_HandleTypeDef  hspi2;     /* 总线1 (FSG_SPI_BUS1) */
I2C_HandleTypeDef  hi2c2;     /* board IMU: I2C_SCL_G/I2C_SDA_G */
UART_HandleTypeDef huart1;    /* 发往 ESP32 (FSG_ESP_UART) */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART1_UART_Init(void);

#define LED_PORT   GPIOE          /* Nucleo-H743ZI2 LD2 (黄) = PE1 */
#define LED_PIN    GPIO_PIN_1

int main(void)
{
    SCB_EnableICache();           /* 开 ICache；DCache 保持关闭以简化 DMA 一致性 */

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_I2C2_Init();
    MX_USART1_UART_Init();

    acquisition_init();           /* DWT 时基 + 绑定总线 + 节点 probe + 链路 */

    /* 固定频率主循环（SysTick 软节拍方案 A；更稳的 TIM 方案见 acquisition.c） */
    const uint32_t period_ms = (FSG_ODR_HZ >= 1u) ? (1000u / FSG_ODR_HZ) : 10u;
    uint32_t next = HAL_GetTick();
    uint32_t led_t = next;

    while (1) {
        uint32_t now = HAL_GetTick();
        if ((now - next) >= period_ms) {
            next += period_ms;
            acquisition_tick();
        }
        if ((now - led_t) >= 500u) {        /* 1Hz 心跳，指示固件在跑 */
            led_t = now;
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        }
    }
}

/* ============================ 时钟：480 MHz ============================ *
 * HSE 8MHz -> PLL1 -> SYSCLK 480MHz。
 *   M=4 -> 2MHz, N=480 -> VCO 960MHz, P=2 -> 480MHz(CPU), Q=8 -> 120MHz(SPI内核)
 *   HCLK=240, APBx=120。VOS0 + FLASH_LATENCY_4。
 *
 * ⚠ HSE 来源（自研 H750IBK6 板必改）：
 *   - 外部晶振：HSEState = RCC_HSE_ON；并按晶振频率改 PLLM 使 PLL 输入落在 1~2MHz
 *     例：晶振 25MHz -> PLLM=5 (5MHz) 不在 2~4 区间则改 PLLRGE；或晶振 8MHz -> PLLM=4(本例)。
 *   - 无源/有源时钟输入(如本例 Nucleo ST-Link MCO 8MHz)：HSEState = RCC_HSE_BYPASS。
 *   同时把 hal_conf 的 HSE_VALUE 改成实际频率。
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       osc    = {0};
    RCC_ClkInitTypeDef       clk    = {0};
    RCC_PeriphCLKInitTypeDef periph = {0};

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) { }

    osc.OscillatorType   = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState         = RCC_HSE_BYPASS;   /* TODO 自研板若用晶振改 RCC_HSE_ON */
    osc.PLL.PLLState     = RCC_PLL_ON;
    osc.PLL.PLLSource    = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM         = 4;                /* TODO 按 HSE_VALUE 调整：HSE/PLLM ∈ [1,2]MHz */
    osc.PLL.PLLN         = 480;
    osc.PLL.PLLP         = 2;
    osc.PLL.PLLQ         = 8;        /* PLL1Q = 120MHz 作 SPI123 内核时钟 */
    osc.PLL.PLLR         = 2;
    osc.PLL.PLLRGE       = RCC_PLL1VCIRANGE_1;   /* 输入 2~4 MHz */
    osc.PLL.PLLVCOSEL    = RCC_PLL1VCOWIDE;
    osc.PLL.PLLFRACN     = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                    RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_D3PCLK1;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider  = RCC_SYSCLK_DIV1;    /* CPU 480 */
    clk.AHBCLKDivider  = RCC_HCLK_DIV2;      /* HCLK 240 */
    clk.APB3CLKDivider = RCC_APB3_DIV2;      /* 120 */
    clk.APB1CLKDivider = RCC_APB1_DIV2;      /* 120 */
    clk.APB2CLKDivider = RCC_APB2_DIV2;      /* 120 */
    clk.APB4CLKDivider = RCC_APB4_DIV2;      /* 120 */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }

    /* SPI1/2/3 内核时钟 = PLL1Q (120MHz)；USART1 用默认 PCLK2(120MHz) */
    periph.PeriphClockSelection = RCC_PERIPHCLK_SPI123;
    periph.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) {
        Error_Handler();
    }
}

/* ============================ GPIO ============================ */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* 心跳 LED PE1 */
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
    g.Pin   = LED_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &g);

    /* 74HC154 译码器输出线（cs_decoder.c 默认 DEC0=PA0..4, DEC1=PB0..4）。
     * A0..A3 初始 0，使能 E(=*4) 初始置高 = 全 CS 释放。
     * ⚠ PB3/PB4 默认是 SWO/NJTRST，配为 GPIO 后这两脚的 JTAG 功能失效
     *   （SWD 调试 PA13/PA14 不受影响）。自研板按 PCB 重新分配。 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
    g.Pin   = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_GPIO_Init(GPIOB, &g);
}

/* ============================ DMA ============================ *
 * DMA 控制器时钟与 USART1_TX 流(DMA1_Stream0)的初始化、NVIC、LINKDMA
 * 在 HAL_UART_MspInit()(stm32h7xx_hal_msp.c) 中完成。此处留空。 */
static void MX_DMA_Init(void) { }

/* ============================ SPI ============================ */
static void spi_common_init(SPI_HandleTypeDef *h, SPI_TypeDef *inst)
{
    h->Instance               = inst;
    h->Init.Mode              = SPI_MODE_MASTER;
    h->Init.Direction         = SPI_DIRECTION_2LINES;
    h->Init.DataSize          = SPI_DATASIZE_8BIT;
    h->Init.CLKPolarity       = SPI_POLARITY_HIGH;   /* SPI mode 3 (LSM6DSOX/LIS3MDL) */
    h->Init.CLKPhase          = SPI_PHASE_2EDGE;
    h->Init.NSS               = SPI_NSS_SOFT;        /* CS 由 74HC154 外部译码 */
    h->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;  /* 120/16 = 7.5MHz (≤10) */
    h->Init.FirstBit          = SPI_FIRSTBIT_MSB;
    h->Init.TIMode            = SPI_TIMODE_DISABLE;
    h->Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    h->Init.CRCPolynomial     = 0x0;
    h->Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
    h->Init.NSSPolarity       = SPI_NSS_POLARITY_LOW;
    h->Init.FifoThreshold     = SPI_FIFO_THRESHOLD_01DATA;
    h->Init.MasterSSIdleness         = SPI_MASTER_SS_IDLENESS_00CYCLE;
    h->Init.MasterInterDataIdleness  = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    h->Init.MasterReceiverAutoSusp   = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    h->Init.MasterKeepIOState        = SPI_MASTER_KEEP_IO_STATE_DISABLE;
    h->Init.IOSwap                   = SPI_IO_SWAP_DISABLE;
    if (HAL_SPI_Init(h) != HAL_OK) {
        Error_Handler();
    }
}
static void MX_SPI1_Init(void) { spi_common_init(&hspi1, SPI1); }
static void MX_SPI2_Init(void) { spi_common_init(&hspi2, SPI2); }

/* ============================ I2C2 ============================ */
static void MX_I2C2_Init(void)
{
    hi2c2.Instance              = I2C2;
    hi2c2.Init.Timing           = FSG_IMU_I2C_TIMING;
    hi2c2.Init.OwnAddress1      = 0;
    hi2c2.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2      = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK) {
        Error_Handler();
    }
}

/* ============================ USART1 ============================ */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance                    = USART1;
    huart1.Init.BaudRate               = FSG_UART_BAUD;     /* 3 Mbps */
    huart1.Init.WordLength             = UART_WORDLENGTH_8B;
    huart1.Init.StopBits               = UART_STOPBITS_1;
    huart1.Init.Parity                 = UART_PARITY_NONE;
    huart1.Init.Mode                   = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling           = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
    HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8);
    HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8);
    HAL_UARTEx_DisableFifoMode(&huart1);
}

/* ============================ 错误处理 ============================ */
void Error_Handler(void)
{
    __disable_irq();
    /* 点亮 LED 常亮指示错误 */
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
}
#endif
