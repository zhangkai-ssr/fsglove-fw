/*
 * app_main.c — ESP32 Wi-Fi 回传端入口
 *
 * 流程：
 *   nvs_flash_init  (Wi-Fi 校准/配置存储依赖 NVS)
 *   -> wifi_sta_start + wait_connected
 *   -> net_sender_init (UDP)
 *   -> 建帧队列 -> stm_link_start(队列)
 *   -> 主循环 xQueueReceive 取帧 -> net_sender_send，并周期打印统计。
 */
#include "fsg_config.h"
#include "wifi_sta.h"
#include "net_sender.h"
#include "stm_link.h"
#include "fsglove_protocol.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "app_main";

void app_main(void)
{
    /* 1. NVS（Wi-Fi 驱动依赖） */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    /* 2. Wi-Fi STA，等连接成功 */
    ESP_ERROR_CHECK(wifi_sta_start());
    if (!wifi_sta_wait_connected(-1)) {     /* 无限等待，断线由内部自动重连 */
        ESP_LOGE(TAG, "Wi-Fi 连接失败");
        return;
    }

    /* 3. UDP 发送端 */
    ESP_ERROR_CHECK(net_sender_init(FSG_HOST_IP, FSG_HOST_PORT));

    /* 4. 帧队列 + UART 接收任务 */
    QueueHandle_t frame_q = xQueueCreate(FSG_FRAME_QUEUE_LEN, sizeof(fsg_frame_t));
    if (frame_q == NULL) {
        ESP_LOGE(TAG, "创建帧队列失败");
        return;
    }
    ESP_ERROR_CHECK(stm_link_start(frame_q));

    ESP_LOGI(TAG, "初始化完成，开始转发帧 (帧长 %u 字节)", (unsigned)FSG_FRAME_SIZE);

    /* 5. 主循环：取帧 -> UDP 发送 -> 周期统计 */
    fsg_frame_t frame;
    uint32_t    rx_count   = 0;      /* 本统计窗口内收到的帧数      */
    uint32_t    sent_count = 0;      /* 本窗口内成功发出的帧数      */
    uint32_t    drop_seq   = 0;      /* 累计基于 seq 检测到的丢帧数 */
    uint32_t    last_seq    = 0;
    bool        have_seq    = false;
    int64_t     last_log_us = esp_timer_get_time();

    for (;;) {
        if (xQueueReceive(frame_q, &frame, pdMS_TO_TICKS(1000)) == pdTRUE) {
            rx_count++;

            /* 基于 seq 估算链路丢帧（UART 队列满 + STM32 端均可能丢） */
            if (have_seq) {
                uint32_t gap = frame.header.seq - last_seq;
                if (gap > 1) drop_seq += (gap - 1);
            }
            last_seq = frame.header.seq;
            have_seq = true;

            if (net_sender_send(&frame) == ESP_OK) {
                sent_count++;
            }
        }

        /* 每 ~1s 打印一次统计 */
        int64_t now = esp_timer_get_time();
        int64_t dt_us = now - last_log_us;
        if (dt_us >= 1000000) {
            float fps = (float)rx_count * 1000000.0f / (float)dt_us;
            ESP_LOGI(TAG,
                     "seq=%u rx=%u tx=%u fps=%.1f 丢帧累计=%u wifi=%d",
                     (unsigned)last_seq, (unsigned)rx_count, (unsigned)sent_count,
                     fps, (unsigned)drop_seq, (int)wifi_sta_is_connected());
            rx_count = 0;
            sent_count = 0;
            last_log_us = now;
        }
    }
}
