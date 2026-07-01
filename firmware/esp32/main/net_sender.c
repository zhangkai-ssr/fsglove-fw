/*
 * net_sender.c — UDP 回传
 *
 * 用 lwip BSD socket。UDP 无连接，sendto() 直接发到固定目标。
 * 整帧 FSG_FRAME_SIZE=338 字节远小于 MTU，单个数据报即可承载，不会分片。
 *
 * 若需改为 TCP（可靠但有重传时延）：把 SOCK_DGRAM 改为 SOCK_STREAM，
 * init 里加 connect()，send 里用 send() 替代 sendto()，并处理断连重连。
 * 见 README "可选改 UDP->TCP" 一节。
 */
#include "net_sender.h"

#include "esp_log.h"
#include "lwip/sockets.h"
#include <string.h>

static const char *TAG = "net_sender";

static int                s_sock = -1;
static struct sockaddr_in s_dest;

esp_err_t net_sender_init(const char *ip, uint16_t port)
{
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock < 0) {
        ESP_LOGE(TAG, "创建 UDP socket 失败: errno=%d", errno);
        return ESP_FAIL;
    }

    memset(&s_dest, 0, sizeof(s_dest));
    s_dest.sin_family = AF_INET;
    s_dest.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip, &s_dest.sin_addr) != 1) {
        ESP_LOGE(TAG, "非法目标 IP: %s", ip);
        close(s_sock);
        s_sock = -1;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UDP 目标 = %s:%u", ip, (unsigned)port);
    return ESP_OK;
}

esp_err_t net_sender_send(const fsg_frame_t *frame)
{
    if (s_sock < 0 || frame == NULL) {
        return ESP_FAIL;
    }

    int sent = sendto(s_sock, frame, FSG_FRAME_SIZE, 0,
                      (struct sockaddr *)&s_dest, sizeof(s_dest));
    if (sent != FSG_FRAME_SIZE) {
        ESP_LOGW(TAG, "UDP 发送异常: 期望 %u 实发 %d errno=%d",
                 (unsigned)FSG_FRAME_SIZE, sent, errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}
