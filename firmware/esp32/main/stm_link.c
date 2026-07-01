/*
 * stm_link.c — STM32H7 UART 链路接收与帧同步
 *
 * 帧同步算法（字节流 -> 帧）：
 *   1. 把读到的字节追加进一个滑动缓冲。
 *   2. 在缓冲里找 2 字节 magic (FSG_FRAME_MAGIC, 小端 = 0x53 0x47)。
 *   3. 找到后等缓冲攒够 FSG_FRAME_SIZE 字节。
 *   4. 用 fsg_frame_check() 验 CRC：
 *        - 通过 -> 整帧投递队列，缓冲整体前移一帧。
 *        - 失败 -> 滑动 1 字节（丢弃当前 magic 的首字节）重新找 magic。
 *   这样即使中途丢字节、错位，也能在 1 帧内重新对齐。
 */
#include <string.h>

#include "stm_link.h"
#include "fsg_config.h"
#include "fsglove_protocol.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "stm_link";

/* magic 小端字节：低字节在前 */
#define MAGIC_LO ((uint8_t)(FSG_FRAME_MAGIC & 0xFF))   /* 0x53 'S' */
#define MAGIC_HI ((uint8_t)(FSG_FRAME_MAGIC >> 8))     /* 0x47 'G' */

/* 解析缓冲：至少能放下两整帧，给滑动留余量 */
#define PARSE_BUF_SIZE (FSG_FRAME_SIZE * 2)

static QueueHandle_t s_out_q;

/* 在 buf[0..len) 中找 magic 起始下标；找不到返回 -1。
 * 若只剩最后 1 字节恰为 MAGIC_LO，无法确认，返回该下标让上层保留等待下一字节。 */
static int find_magic(const uint8_t *buf, int len)
{
    for (int i = 0; i + 1 < len; i++) {
        if (buf[i] == MAGIC_LO && buf[i + 1] == MAGIC_HI) {
            return i;
        }
    }
    /* 末尾残留可能是半个 magic，保留它 */
    if (len > 0 && buf[len - 1] == MAGIC_LO) {
        return len - 1;
    }
    return -1;
}

static void stm_link_task(void *arg)
{
    static uint8_t parse_buf[PARSE_BUF_SIZE];
    int buf_len = 0;                 /* parse_buf 中有效字节数 */

    uint8_t rx_chunk[512];
    fsg_frame_t frame;

    ESP_LOGI(TAG, "stm_link_task 启动，等待 STM32 数据帧 (帧长 %u 字节)", (unsigned)FSG_FRAME_SIZE);

    for (;;) {
        /* 从 UART 读一批字节（阻塞，带超时以便周期性返回） */
        int n = uart_read_bytes(FSG_UART_PORT, rx_chunk, sizeof(rx_chunk),
                                pdMS_TO_TICKS(20));
        if (n <= 0) {
            continue;
        }

        /* 追加到解析缓冲；若放不下，先做一次紧缩（理论上不应发生，做保护） */
        if (buf_len + n > PARSE_BUF_SIZE) {
            /* 丢弃最旧的数据，给新数据腾位 */
            int drop = (buf_len + n) - PARSE_BUF_SIZE;
            if (drop > buf_len) drop = buf_len;
            memmove(parse_buf, parse_buf + drop, buf_len - drop);
            buf_len -= drop;
        }
        int copy = n;
        if (copy > PARSE_BUF_SIZE - buf_len) copy = PARSE_BUF_SIZE - buf_len;
        memcpy(parse_buf + buf_len, rx_chunk, copy);
        buf_len += copy;

        /* 尽可能多地从缓冲里解析出完整帧 */
        for (;;) {
            int idx = find_magic(parse_buf, buf_len);
            if (idx < 0) {
                /* 没有 magic，整段丢弃 */
                buf_len = 0;
                break;
            }
            if (idx > 0) {
                /* 把 magic 移到缓冲头部 */
                memmove(parse_buf, parse_buf + idx, buf_len - idx);
                buf_len -= idx;
            }
            if (buf_len < FSG_FRAME_SIZE) {
                /* 数据还不够一整帧，等下次 UART 读入 */
                break;
            }

            memcpy(&frame, parse_buf, FSG_FRAME_SIZE);
            if (fsg_frame_check(&frame)) {
                /* 有效帧：投递队列。队列满则丢弃（低延迟优先，丢帧可接受） */
                if (s_out_q != NULL &&
                    xQueueSend(s_out_q, &frame, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "帧队列已满，丢弃 seq=%u", (unsigned)frame.header.seq);
                }
                /* 缓冲前移一整帧 */
                memmove(parse_buf, parse_buf + FSG_FRAME_SIZE,
                        buf_len - FSG_FRAME_SIZE);
                buf_len -= FSG_FRAME_SIZE;
            } else {
                /* CRC 失败：滑动 1 字节重新找 magic（可能是伪 magic 或错位） */
                memmove(parse_buf, parse_buf + 1, buf_len - 1);
                buf_len -= 1;
            }
        }
    }
}

esp_err_t stm_link_start(QueueHandle_t out_q)
{
    s_out_q = out_q;

    const uart_config_t cfg = {
        .baud_rate  = FSG_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(FSG_UART_PORT,
                                        FSG_UART_RX_BUF_SIZE,
                                        FSG_UART_TX_BUF_SIZE,
                                        0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(FSG_UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(FSG_UART_PORT,
                                 FSG_UART_TX_PIN, FSG_UART_RX_PIN,
                                 FSG_UART_RTS_PIN, FSG_UART_CTS_PIN));

    ESP_LOGI(TAG, "UART%d 初始化完成: baud=%d TX=%d RX=%d",
             FSG_UART_PORT, FSG_UART_BAUD, FSG_UART_TX_PIN, FSG_UART_RX_PIN);

    BaseType_t ok = xTaskCreate(stm_link_task, "stm_link", 4096, NULL,
                                10, NULL);
    return (ok == pdPASS) ? ESP_OK : ESP_FAIL;
}
