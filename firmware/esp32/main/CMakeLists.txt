# ESP32 应用组件注册
# INCLUDE_DIRS 含 "../../common" 以便 include 三端共享的 fsglove_protocol.h
idf_component_register(
    SRCS
        "app_main.c"
        "stm_link.c"
        "wifi_sta.c"
        "net_sender.c"
    INCLUDE_DIRS
        "."
        "../../common"
    REQUIRES
        driver          # UART
        esp_wifi        # Wi-Fi
        esp_event       # 事件循环
        esp_netif       # netif
        nvs_flash       # NVS
        lwip            # BSD socket
        esp_timer       # 统计计时
)
