/*
 * spi_bus.c — SPI 总线抽象实现
 */
#include "spi_bus.h"
#include "cs_decoder.h"

/* 阻塞传输超时 (ms) */
#define SPI_BUS_TIMEOUT_MS   10u

void spi_bus_init(spi_bus_t *bus, SPI_HandleTypeDef *hspi, uint8_t bus_id)
{
    bus->hspi   = hspi;
    bus->bus_id = bus_id;
}

int spi_bus_transfer(spi_bus_t *bus, uint8_t cs_addr,
                     const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    cs_decoder_select(bus->bus_id, cs_addr);

    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(
        bus->hspi, (uint8_t *)tx, rx, len, SPI_BUS_TIMEOUT_MS);

    cs_decoder_deselect(bus->bus_id);

    return (st == HAL_OK) ? 0 : -1;
}

int spi_bus_transfer_dma(spi_bus_t *bus, uint8_t cs_addr,
                         const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    /* TODO: DMA 版本
     *   1) cs_decoder_select(bus->bus_id, cs_addr)
     *   2) HAL_SPI_TransmitReceive_DMA(bus->hspi, tx, rx, len)
     *   3) 在 HAL_SPI_TxRxCpltCallback() 中 cs_decoder_deselect 并置完成标志
     * 需注意：DMA 缓冲区应放非 cache / 已做 cache 维护的内存（H7 D-Cache）。
     */
    (void)bus; (void)cs_addr; (void)tx; (void)rx; (void)len;
    return -1;   /* 未实现 */
}

int spi_bus_xfer_cb(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    spi_chan_t *ch = (spi_chan_t *)ctx;
    return spi_bus_transfer(ch->bus, ch->cs_addr, tx, rx, len);
}
