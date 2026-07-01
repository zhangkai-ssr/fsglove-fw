/*
 * wifi_sta.h — Wi-Fi STA 模式连接管理
 */
#ifndef WIFI_STA_H
#define WIFI_STA_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 netif/事件循环/Wi-Fi 驱动并发起连接（异步）。断线自动重连。 */
esp_err_t wifi_sta_start(void);

/* 阻塞等待连接成功（拿到 IP）。timeout_ms < 0 表示无限等待。
 * 返回 true=已连接，false=超时。 */
bool wifi_sta_wait_connected(int timeout_ms);

/* 查询当前是否已连接并拿到 IP。 */
bool wifi_sta_is_connected(void);

#ifdef __cplusplus
}
#endif
#endif /* WIFI_STA_H */
