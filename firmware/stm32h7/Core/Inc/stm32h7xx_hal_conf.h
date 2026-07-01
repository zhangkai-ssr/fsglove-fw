/*
 * stm32h7xx_hal_conf.h — FSGlove STM32H7 HAL 配置
 *
 * 裁剪自 ST 标准模板：只启用本工程用到的 HAL 模块，
 * 其余模块的 .c 文件因文件级 #ifdef 保护不会产生代码。
 * 关键：HSE_VALUE = 8 MHz（Nucleo-H743ZI2 由 ST-Link MCO 提供，HSE BYPASS）。
 */
#ifndef STM32H7xx_HAL_CONF_H
#define STM32H7xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########################## 模块使能 ########################## */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED

/* ########################## 振荡器/电源参数 ########################## */
#if !defined  (HSE_VALUE)
#define HSE_VALUE             8000000UL   /* 外部时钟 8 MHz（ST-Link MCO, HSE BYPASS） */
#endif
#if !defined  (HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT   100UL
#endif
#if !defined  (CSI_VALUE)
#define CSI_VALUE             4000000UL
#endif
#if !defined  (HSI_VALUE)
#define HSI_VALUE             64000000UL
#endif
#if !defined  (LSI_VALUE)
#define LSI_VALUE             32000UL
#endif
#if !defined  (LSE_VALUE)
#define LSE_VALUE             32768UL
#endif
#if !defined  (LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT   5000UL
#endif
#if !defined  (EXTERNAL_CLOCK_VALUE)
#define EXTERNAL_CLOCK_VALUE  12288000UL
#endif

#define VDD_VALUE                  3300UL
#define TICK_INT_PRIORITY          15UL
#define USE_RTOS                   0U
#define USE_SPI_CRC                0U
#define USE_HAL_I2C_REGISTER_CALLBACKS   0U
#define USE_HAL_SPI_REGISTER_CALLBACKS  0U
#define USE_HAL_UART_REGISTER_CALLBACKS 0U

/* ########################## Assert ########################## */
/* #define USE_FULL_ASSERT    1U */

/* ########################## 包含 HAL 模块头 ########################## */
#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32h7xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32h7xx_hal_gpio.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32h7xx_hal_dma.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32h7xx_hal_cortex.h"
#endif
#ifdef HAL_EXTI_MODULE_ENABLED
#include "stm32h7xx_hal_exti.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32h7xx_hal_flash.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32h7xx_hal_pwr.h"
#endif
#ifdef HAL_SPI_MODULE_ENABLED
#include "stm32h7xx_hal_spi.h"
#endif
#ifdef HAL_I2C_MODULE_ENABLED
#include "stm32h7xx_hal_i2c.h"
#include "stm32h7xx_hal_i2c_ex.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "stm32h7xx_hal_uart.h"
#endif

/* ########################## Assert 宏 ########################## */
#ifdef  USE_FULL_ASSERT
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif
#endif /* STM32H7xx_HAL_CONF_H */
