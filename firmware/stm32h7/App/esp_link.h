/*
 * esp_link.h — 内部链路抽象（STM32 -> ESP32）
 *
 * 默认实现：UART + DMA 把整帧 338B 原样发出。
 * 接收端（ESP32/上位机）靠 [magic 0x4753] + 固定 338B 长度 + CRC16 自同步，
 * 无需额外分隔符/转义。
 *
 * 接口抽象：日后可换成 SPI 从机链路，只需替换 esp_link.c。
 */
#ifndef ESP_LINK_H
#define ESP_LINK_H

#include "../../common/fsglove_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化链路（DMA 就绪检查等） */
void esp_link_init(void);

/* 发送一整帧。返回 0 成功（已提交 DMA），<0 忙/失败。
 * 调用方应保证上一帧 DMA 已完成或使用双缓冲（见 .c 说明）。 */
int esp_link_send_frame(const fsg_frame_t *frame);

#ifdef __cplusplus
}
#endif
#endif /* ESP_LINK_H */
