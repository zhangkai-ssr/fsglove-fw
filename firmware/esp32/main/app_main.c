#include "fsg_config.h"
#include "fsglove_protocol.h"
#include "net_sender.h"
#include "stm_link.h"
#include "wifi_sta.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "nvs_flash.h"

static const char *TAG = "app_main";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(wifi_sta_start());
    if (!wifi_sta_wait_connected(-1)) {
        ESP_LOGE(TAG, "wifi connect failed");
        return;
    }

    ESP_ERROR_CHECK(net_sender_init(FSG_HOST_IP, FSG_HOST_PORT));

    QueueHandle_t frame_q = xQueueCreate(FSG_FRAME_QUEUE_LEN, sizeof(fsg_frame_t));
    if (frame_q == NULL) {
        ESP_LOGE(TAG, "frame queue create failed");
        return;
    }
    ESP_ERROR_CHECK(stm_link_start(frame_q));

    ESP_LOGI(TAG, "relay ready, frame_size=%u", (unsigned)FSG_FRAME_SIZE);

    fsg_frame_t frame;
    uint32_t rx_count = 0;
    uint32_t sent_count = 0;
    uint32_t drop_seq = 0;
    uint32_t last_seq = 0;
    bool have_seq = false;
    int64_t last_log_us = esp_timer_get_time();

    for (;;) {
        if (xQueueReceive(frame_q, &frame, pdMS_TO_TICKS(1000)) == pdTRUE) {
            rx_count++;

            if (have_seq) {
                uint32_t gap = frame.header.seq - last_seq;
                if (gap > 1) {
                    drop_seq += gap - 1;
                }
            }
            last_seq = frame.header.seq;
            have_seq = true;

            if (net_sender_send(&frame) == ESP_OK) {
                sent_count++;
            }
        }

        int64_t now = esp_timer_get_time();
        int64_t dt_us = now - last_log_us;
        if (dt_us >= 1000000) {
            float fps = (float)rx_count * 1000000.0f / (float)dt_us;
            ESP_LOGI(TAG, "seq=%u rx=%u tx=%u fps=%.1f drop_seq=%u wifi=%d",
                     (unsigned)last_seq, (unsigned)rx_count, (unsigned)sent_count,
                     fps, (unsigned)drop_seq, (int)wifi_sta_is_connected());
            rx_count = 0;
            sent_count = 0;
            last_log_us = now;
        }
    }
}
