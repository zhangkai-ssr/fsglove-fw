/*
 * fsglove_config.h — FSGlove v2 采集端编译期配置
 *
 * 集中管理所有编译开关、时钟/波特率、ODR 以及
 * “节点 -> {总线, 片选地址}” 的硬件映射表。
 * 改硬件只需改本文件，不动驱动逻辑。
 */
#ifndef FSGLOVE_CONFIG_H
#define FSGLOVE_CONFIG_H

#include <stdint.h>
#include "../../common/fsglove_protocol.h"   /* 唯一权威协议契约 */

/* ============================ 编译开关 ============================ */

/* 磁力计开关：LIS3MDL 已停产。
 *   1 = 启用磁力计读取（9 轴）
 *   0 = 关闭，退化为纯 6 轴（mag[]=0，status 的 MAG 位不置位） */
#ifndef FSG_ENABLE_MAG
#define FSG_ENABLE_MAG      1
#endif

/* 手别：编译进固件，写入帧头 hand 字段 */
#ifndef FSG_HAND
#define FSG_HAND            FSG_HAND_RIGHT     /* FSG_HAND_LEFT / FSG_HAND_RIGHT */
#endif

/* 输出数据率 (Hz)：100 默认，200 可选。需与 IMU ODR 寄存器配置一致 */
#ifndef FSG_ODR_HZ
#define FSG_ODR_HZ          100u               /* 100 / 200 */
#endif

/* ============================ 链路参数 ============================ */

/* SPI 总线时钟 (Hz)：方案 SCLK 10MHz。实际预分频由 CubeMX 设定，
 * 此处仅作记录/校验用途。 */
#define FSG_SPI_CLK_HZ      10000000u

/* 发往 ESP32 的 UART 波特率：默认 3 Mbps
 *   338B/帧 @ 200Hz ≈ 67.6 KB/s ≈ 676 Kbps，3M 余量充足 */
#ifndef FSG_UART_BAUD
#define FSG_UART_BAUD       3000000u
#endif

/* I2C2 timing for the board IMU. Default targets ~400 kHz with 120 MHz PCLK1.
 * Tune on a scope if the board clock tree changes. */
#ifndef FSG_IMU_I2C_TIMING
#define FSG_IMU_I2C_TIMING  0x5042131Du
#endif

/* ============================ 节点映射表 ============================ *
 * 16 节点布局（方案）：
 *   - 每根手指 1 条 FPC × 3 节点 = 5 指 × 3 = 15 节点
 *   - 手背参考 1 节点
 *   - 2 条硬件 SPI 总线，各挂 8 节点
 *   - 片选经 74HC154(4->16) 译码：每条总线 1 片，4 根地址线选 16 路 CS，
 *     覆盖该总线 8 节点 × 2 CS(CS_A=LSM6DSOX / CS_M=LIS3MDL)
 *
 * cs_addr_ag / cs_addr_mag = 74HC154 的 4bit 地址 (0..15)。
 * bus = 0 / 1。
 *
 * ⚠️ 下表数值为占位，须按实际 PCB 走线/译码器通道核对修改。 TODO
 */
typedef struct {
    uint8_t bus;            /* 所属 SPI 总线 0..FSG_SPI_BUS_COUNT-1   */
    uint8_t cs_addr_ag;     /* 74HC154 地址：LSM6DSOX 片选 (加/陀)     */
    uint8_t cs_addr_mag;    /* 74HC154 地址：LIS3MDL  片选 (磁)        */
} fsg_node_map_t;

/* 约定：同一节点的 AG/MAG 片选成对，地址 = 2*local + {0,1}
 * 总线 0 承载节点 0..7，总线 1 承载节点 8..15。
 * TODO: 以实际 PCB 通道映射为准，逐行核对。 */
static const fsg_node_map_t fsg_node_map[FSG_NODE_COUNT] = {
    /* node, bus, cs_ag, cs_mag   说明（占位）            */
    /* 0 */ { 0,  0,  1 },   /* 拇指 近   bus0           TODO */
    /* 1 */ { 0,  2,  3 },   /* 拇指 中   bus0           TODO */
    /* 2 */ { 0,  4,  5 },   /* 拇指 远   bus0           TODO */
    /* 3 */ { 0,  6,  7 },   /* 食指 近   bus0           TODO */
    /* 4 */ { 0,  8,  9 },   /* 食指 中   bus0           TODO */
    /* 5 */ { 0, 10, 11 },   /* 食指 远   bus0           TODO */
    /* 6 */ { 0, 12, 13 },   /* 中指 近   bus0           TODO */
    /* 7 */ { 0, 14, 15 },   /* 中指 中   bus0           TODO */
    /* 8 */ { 1,  0,  1 },   /* 中指 远   bus1           TODO */
    /* 9 */ { 1,  2,  3 },   /* 无名 近   bus1           TODO */
    /*10 */ { 1,  4,  5 },   /* 无名 中   bus1           TODO */
    /*11 */ { 1,  6,  7 },   /* 无名 远   bus1           TODO */
    /*12 */ { 1,  8,  9 },   /* 小指 近   bus1           TODO */
    /*13 */ { 1, 10, 11 },   /* 小指 中   bus1           TODO */
    /*14 */ { 1, 12, 13 },   /* 小指 远   bus1           TODO */
    /*15 */ { 1, 14, 15 },   /* 手背 参考 bus1           TODO */
};

#endif /* FSGLOVE_CONFIG_H */
