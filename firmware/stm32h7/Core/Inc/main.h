/*
 * main.h — 工程主头
 *
 * 汇总 HAL 头、外设句柄 extern 与 App 层使用的逻辑别名。
 * 句柄实体在 main.c 定义；引脚分配见 stm32h7xx_hal_msp.c。
 */
#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

/* ---- 外设句柄（main.c 定义） ---- */
extern SPI_HandleTypeDef  hspi1;
extern SPI_HandleTypeDef  hspi2;
extern I2C_HandleTypeDef  hi2c2;
extern UART_HandleTypeDef huart1;

/* ---- App 层逻辑别名（改硬件映射只动这里，不动驱动） ---- */
#define FSG_SPI_BUS0   hspi1     /* SPI 总线 0 */
#define FSG_SPI_BUS1   hspi2     /* SPI 总线 1 */
#define FSG_IMU_I2C    hi2c2     /* U4 LSM6DSOX: PF1/PF0 I2C2 */
#define FSG_ESP_UART   huart1    /* 发往 ESP32 的 UART */

void Error_Handler(void);

#ifdef __cplusplus
}
#endif
#endif /* MAIN_H */
