/*
 * stm32h7xx_hal_msp.c — HAL 底层硬件初始化 (MSP)
 *
 * 针对 Nucleo-H743ZI2 的引脚分配（自研板请按 PCB 改）：
 *   SPI1 : SCK=PA5  MISO=PA6  MOSI=PA7   (AF5)
 *   SPI2 : SCK=PB13 MISO=PB14 MOSI=PB15  (AF5)  ⚠ PB14=Nucleo LD3, 自研板可改
 *   USART1: TX=PA9  RX=PA10              (AF7) -> 发往 ESP32
 *   USART1_TX DMA: DMA1_Stream0 (request USART1_TX)
 */
#include "main.h"

/* USART1 TX 的 DMA 句柄（stm32h7xx_it.c 中 extern 引用） */
DMA_HandleTypeDef hdma_usart1_tx;

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    /* 注：STM32H7 的 PWR 始终供电，无 __HAL_RCC_PWR_CLK_ENABLE 宏（不同于 F4 系列） */
}

/* ---------------------------- SPI ---------------------------- */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef g = {0};

    if (hspi->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        /* PA5 SCK, PA6 MISO, PA7 MOSI */
        g.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &g);
    } else if (hspi->Instance == SPI2) {
        __HAL_RCC_SPI2_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        /* PB13 SCK, PB14 MISO, PB15 MOSI */
        g.Pin       = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOB, &g);
    }
}

/* ---------------------------- I2C ---------------------------- */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef g = {0};

    if (hi2c->Instance == I2C2) {
        __HAL_RCC_I2C2_CLK_ENABLE();
        __HAL_RCC_GPIOF_CLK_ENABLE();

        /* Netlist: I2C_SDA_G=PF0, I2C_SCL_G=PF1. */
        g.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
        g.Mode      = GPIO_MODE_AF_OD;
        g.Pull      = GPIO_PULLUP;
        g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(GPIOF, &g);
    }
}

/* ---------------------------- UART ---------------------------- */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef g = {0};

    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        /* PA9 TX, PA10 RX (AF7) */
        g.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_PULLUP;
        g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &g);

        /* USART1_TX -> DMA1 Stream0 */
        hdma_usart1_tx.Instance                 = DMA1_Stream0;
        hdma_usart1_tx.Init.Request             = DMA_REQUEST_USART1_TX;
        hdma_usart1_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode                = DMA_NORMAL;
        hdma_usart1_tx.Init.Priority            = DMA_PRIORITY_LOW;
        hdma_usart1_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(huart, hdmatx, hdma_usart1_tx);

        HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
        HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
}
