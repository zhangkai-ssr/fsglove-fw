/*
 * spi_bus.h — 一条 SPI 总线的抽象
 *
 * 把 “HAL SPI 句柄 + 该总线的 74HC154 译码器” 绑在一起。
 * 一次 transfer = 选片选 -> 全双工收发 -> 释放片选。
 * IMU 驱动只持有 spi_bus_t* 与 cs_addr，不感知 HAL。
 */
#ifndef SPI_BUS_H
#define SPI_BUS_H

#include <stdint.h>
#include "main.h"   /* SPI_HandleTypeDef */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    SPI_HandleTypeDef *hspi;     /* CubeMX 生成的 SPI 句柄 */
    uint8_t            bus_id;   /* 0..FSG_SPI_BUS_COUNT-1，用于选译码器 */
} spi_bus_t;

/* 传给 IMU 驱动的收发上下文：驱动只见 void*，内部还原为本结构。
 * imu_node 在每次读某传感器前把 {bus, cs_addr} 填好。 */
typedef struct {
    spi_bus_t *bus;
    uint8_t    cs_addr;
} spi_chan_t;

/* 绑定句柄与总线号 */
void spi_bus_init(spi_bus_t *bus, SPI_HandleTypeDef *hspi, uint8_t bus_id);

/* 阻塞全双工传输：
 *   cs_decoder_select(bus_id, cs_addr)
 *   -> HAL_SPI_TransmitReceive(...)
 *   -> cs_decoder_deselect(bus_id)
 * 返回 0 成功，<0 失败。tx/rx 长度均为 len。 */
int spi_bus_transfer(spi_bus_t *bus, uint8_t cs_addr,
                     const uint8_t *tx, uint8_t *rx, uint16_t len);

/* DMA 版接口（留待启用）：非阻塞，完成由 HAL_SPI_TxRxCpltCallback 通知，
 * 回调中需调用 cs_decoder_deselect。 TODO: 实现 + 完成信号量/标志 */
int spi_bus_transfer_dma(spi_bus_t *bus, uint8_t cs_addr,
                         const uint8_t *tx, uint8_t *rx, uint16_t len);

/* 提供给 IMU 驱动 (lsm6dsox/lis3mdl) 的 xfer 回调适配器：
 * ctx 实为 spi_chan_t*，内部按 {bus, cs_addr} 调用 spi_bus_transfer。 */
int spi_bus_xfer_cb(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len);

#ifdef __cplusplus
}
#endif
#endif /* SPI_BUS_H */
