/*
 * stm_link.h — STM32H7 UART 链路接收
 *
 * 职责：初始化 UART，启动后台任务从字节流中做帧同步（magic + 定长 + CRC），
 *       把完整的 fsg_frame_t 投递到调用方提供的 FreeRTOS 队列。
 */
#ifndef STM_LINK_H
#define STM_LINK_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 初始化 UART 并启动接收任务。
 * out_q : 元素类型为 fsg_frame_t 的队列，收到完整有效帧后写入。
 * 返回 ESP_OK 表示成功。
 */
esp_err_t stm_link_start(QueueHandle_t out_q);

#ifdef __cplusplus
}
#endif
#endif /* STM_LINK_H */
