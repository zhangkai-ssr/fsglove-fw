#ifndef FSG_CONFIG_H
#define FSG_CONFIG_H

#include "driver/uart.h"

/* Wi-Fi STA defaults. Override with compiler definitions for real boards. */
#ifndef FSG_WIFI_SSID
#define FSG_WIFI_SSID        "your-ap-ssid"
#endif
#ifndef FSG_WIFI_PASS
#define FSG_WIFI_PASS        "your-ap-password"
#endif
#ifndef FSG_WIFI_MAX_RETRY
#define FSG_WIFI_MAX_RETRY   0                   /* 0 = retry forever */
#endif

/* Host UDP target. */
#ifndef FSG_HOST_IP
#define FSG_HOST_IP          "192.168.1.100"
#endif
#ifndef FSG_HOST_PORT
#define FSG_HOST_PORT        18888
#endif

/* UART link to STM32H7. */
#define FSG_UART_PORT        UART_NUM_1
#define FSG_UART_BAUD        3000000
#define FSG_UART_TX_PIN      17                  /* ESP32 TX -> STM32 RX */
#define FSG_UART_RX_PIN      16                  /* ESP32 RX <- STM32 TX */
#define FSG_UART_RTS_PIN     UART_PIN_NO_CHANGE
#define FSG_UART_CTS_PIN     UART_PIN_NO_CHANGE

#define FSG_UART_RX_BUF_SIZE (16 * 1024)
#define FSG_UART_TX_BUF_SIZE 0
#define FSG_FRAME_QUEUE_LEN  8

#endif /* FSG_CONFIG_H */
