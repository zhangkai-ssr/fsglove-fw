/*
 * esp_link.c — UART 链路实现
 */
#include "esp_link.h"
#include "fsglove_config.h"
#include "main.h"

/* 发往 ESP32 的 UART 句柄（CubeMX 生成，main.h extern）。
 * TODO: 改为实际句柄，例如 huart1。波特率在 CubeMX 设为 FSG_UART_BAUD。 */
extern UART_HandleTypeDef FSG_ESP_UART;

void esp_link_init(void)
{
    /* UART/DMA 由 CubeMX MX_USARTx_UART_Init / MX_DMA_Init 完成。
     * 此处可做额外检查或清状态。 TODO: 如需流控/握手在此实现 */
}

int esp_link_send_frame(const fsg_frame_t *frame)
{
    /* 整帧定长 338B 一次性 DMA 发出。
     *
     * 注意（STM32H7 D-Cache）：若帧缓冲位于带 cache 的内存，DMA 前需
     *   SCB_CleanDCache_by_Addr((uint32_t*)frame, FSG_FRAME_SIZE);
     * 或将缓冲放入非 cache 的 MPU 区/ .dma_buffer 段。 TODO
     *
     * 双缓冲建议：acquisition 用 ping-pong 两个 fsg_frame_t，
     * 本帧 DMA 进行时填下一帧，避免覆盖正在发送的数据。 */
    HAL_StatusTypeDef st = HAL_UART_Transmit_DMA(
        &FSG_ESP_UART, (uint8_t *)frame, FSG_FRAME_SIZE);

    return (st == HAL_OK) ? 0 : -1;
}
