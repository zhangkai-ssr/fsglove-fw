/*
 * fsg_config.h — ESP32 Wi-Fi 回传端编译期配置
 *
 * 这里用 #define 给出默认值，便于快速上手。
 * 生产环境建议改为 Kconfig (menuconfig)：在 main/Kconfig.projbuild 里声明
 * 对应选项，再用 CONFIG_xxx 替换本文件的宏，即可通过 `idf.py menuconfig`
 * 配置而无需改源码。
 */
#ifndef FSG_CONFIG_H
#define FSG_CONFIG_H

#include "driver/uart.h"

/* ============ Wi-Fi (STA 模式) ============ */
#define FSG_WIFI_SSID        "your-ap-ssid"      /* 待连接 AP 名称   */
#define FSG_WIFI_PASS        "your-ap-password"  /* AP 密码          */
#define FSG_WIFI_MAX_RETRY   0                   /* 0 = 无限重连     */

/* ============ 上位机 (UDP 目标) ============ */
#define FSG_HOST_IP          "192.168.1.100"     /* 上位机 IP        */
#define FSG_HOST_PORT        18888               /* 上位机 UDP 端口  */

/* ============ UART (对接 STM32H7) ============ */
#define FSG_UART_PORT        UART_NUM_1          /* 使用的 UART 控制器 */
#define FSG_UART_BAUD        3000000             /* 波特率，与 STM32 一致 */
#define FSG_UART_TX_PIN      17                  /* ESP32 TX -> STM32 RX */
#define FSG_UART_RX_PIN      16                  /* ESP32 RX <- STM32 TX */
#define FSG_UART_RTS_PIN     UART_PIN_NO_CHANGE
#define FSG_UART_CTS_PIN     UART_PIN_NO_CHANGE

/* UART 驱动 RX 环形缓冲大小（字节）。
 * 3Mbps + 200Hz × 338B ≈ 67KB/s，留足缓冲避免溢出丢字节。 */
#define FSG_UART_RX_BUF_SIZE (16 * 1024)
#define FSG_UART_TX_BUF_SIZE 0                   /* 不主动向 STM32 发，置 0 */

/* ============ 帧队列 ============ */
#define FSG_FRAME_QUEUE_LEN  8                   /* UART->发送 任务间队列深度 */

#endif /* FSG_CONFIG_H */
