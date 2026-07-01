/*
 * acquisition.h — 采集核心
 *
 * 负责：micros() 时基(DWT 周期计数)、节点 probe、按 FSG_ODR_HZ
 * 周期性读 16 节点、组帧(协议)、finalize CRC、交 esp_link 发送。
 */
#ifndef ACQUISITION_H
#define ACQUISITION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化时基(DWT)、SPI 总线、IMU 层并 probe 节点。
 * 须在 HAL 与 CubeMX 外设 init 之后调用。 */
void acquisition_init(void);

/* 当前微秒时间戳（DWT 周期计数换算）。 */
uint32_t acquisition_micros(void);

/* 采一帧：读全 16 节点 -> 填帧头 -> finalize -> 发送。
 * 由主循环按 FSG_ODR_HZ 节拍调用（见 .c 调度说明）。 */
void acquisition_tick(void);

#ifdef __cplusplus
}
#endif
#endif /* ACQUISITION_H */
