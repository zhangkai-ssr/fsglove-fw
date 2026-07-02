#include "wifi_sta.h"
#include "fsg_config.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "wifi_sta";

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t s_wifi_eg;
static volatile bool s_connected;
static int s_retry;

static void set_connected(bool connected)
{
    s_connected = connected;
    if (s_wifi_eg == NULL) {
        return;
    }
    if (connected) {
        xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT);
    } else {
        xEventGroupClearBits(s_wifi_eg, WIFI_CONNECTED_BIT);
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        set_connected(false);
        if (FSG_WIFI_MAX_RETRY == 0 || s_retry < FSG_WIFI_MAX_RETRY) {
            s_retry++;
            ESP_LOGW(TAG, "wifi disconnected, reconnecting (%d)", s_retry);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "wifi reconnect retries exhausted");
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "wifi connected, ip=" IPSTR, IP2STR(&evt->ip_info.ip));
        s_retry = 0;
        set_connected(true);
    }
}

esp_err_t wifi_sta_start(void)
{
    s_wifi_eg = xEventGroupCreate();
    if (s_wifi_eg == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (esp_netif_create_default_wifi_sta() == NULL) {
        return ESP_FAIL;
    }

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi_event, NULL, NULL));

    wifi_config_t wifi_cfg = { 0 };
    strncpy((char *)wifi_cfg.sta.ssid, FSG_WIFI_SSID, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, FSG_WIFI_PASS, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode =
        (FSG_WIFI_PASS[0] == '\0') ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi STA started, ssid=%s", FSG_WIFI_SSID);
    return ESP_OK;
}

bool wifi_sta_wait_connected(int timeout_ms)
{
    if (s_wifi_eg == NULL) {
        return false;
    }
    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    EventBits_t bits = xEventGroupWaitBits(s_wifi_eg, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdTRUE, ticks);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

bool wifi_sta_is_connected(void)
{
    return s_connected;
}
