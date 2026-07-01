/*
 * net_sender.h — UDP 回传到上位机
 */
#ifndef NET_SENDER_H
#define NET_SENDER_H

#include "esp_err.h"
#include "fsglove_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 创建 UDP socket 并固定目标地址 ip:port。返回 ESP_OK 成功。 */
esp_err_t net_sender_init(const char *ip, uint16_t port);

/* 把整帧 fsg_frame_t 作为一个 UDP 数据报发往目标。失败返回 ESP_FAIL 并打日志。 */
esp_err_t net_sender_send(const fsg_frame_t *frame);

#ifdef __cplusplus
}
#endif
#endif /* NET_SENDER_H */
