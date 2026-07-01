/*
 * fsglove_protocol.h  —  FSGlove v2 三端共享线缆协议（STM32H7 / ESP32 / 上位机）
 *
 * 数据流：
 *   16 个 9 轴节点 (LSM6DSOX + LIS3MDL)
 *     -> STM32H7  采集 + 打时间戳/序号 + 打包成 fsg_frame_t
 *     -> 内部链路 (UART/SPI)  原样透传
 *     -> ESP32    Wi-Fi (UDP) 原样转发
 *     -> 上位机    解析 fsg_frame_t -> 姿态融合 / 记录
 *
 * 约定：所有多字节字段一律 **小端 (little-endian)**。
 *       结构体用 1 字节对齐 (#pragma pack)，可直接按内存布局收发。
 *       上位机 (Python) 按本文件标注的偏移/格式用 struct 解析，二者必须一致。
 *
 * 本文件是纯 C 头文件，无依赖，STM32 / ESP32 直接 include。
 */
#ifndef FSGLOVE_PROTOCOL_H
#define FSGLOVE_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 版本与魔数 ---- */
#define FSG_PROTO_VERSION   0x02            /* 协议版本 v2                       */
#define FSG_FRAME_MAGIC     0x4753          /* 'S''G' 小端 = 0x53 0x47, 帧起始    */

/* ---- 规模常量（与硬件方案一致：单手 16 节点） ---- */
#define FSG_NODE_COUNT      16              /* 单手节点数                         */
#define FSG_SPI_BUS_COUNT   2               /* SPI 总线数                         */
#define FSG_NODES_PER_BUS   8               /* 每条总线节点数                     */

/* ---- 手别 ---- */
typedef enum {
    FSG_HAND_LEFT  = 0,
    FSG_HAND_RIGHT = 1,
} fsg_hand_t;

/* ---- 每节点状态位 (status 字段) ---- */
#define FSG_ST_ACCEL_OK     (1u << 0)       /* LSM6DSOX 读回校验通过             */
#define FSG_ST_GYRO_OK      (1u << 1)
#define FSG_ST_MAG_OK       (1u << 2)       /* LIS3MDL 读回校验通过              */
#define FSG_ST_NODE_ONLINE  (1u << 3)       /* WHO_AM_I 命中、节点在线           */
#define FSG_ST_DATA_FRESH   (1u << 4)       /* 本帧为新采样 (非保持上一帧)        */

/* ---- 单节点原始 9 轴数据 (20 字节) ----
 * 偏移(相对节点起始):
 *   0  node_id   u8
 *   1  status    u8
 *   2  accel[3]  i16 le   (LSM6DSOX)
 *   8  gyro[3]   i16 le   (LSM6DSOX)
 *   14 mag[3]    i16 le   (LIS3MDL)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  node_id;          /* 物理/逻辑节点号 0..FSG_NODE_COUNT-1 */
    uint8_t  status;           /* FSG_ST_* 位组合                     */
    int16_t  accel[3];         /* 原始加速度 x/y/z                    */
    int16_t  gyro[3];          /* 原始角速度 x/y/z                    */
    int16_t  mag[3];           /* 原始磁力   x/y/z                    */
} fsg_node_data_t;             /* sizeof = 20                         */

/* ---- 帧头 (16 字节) ----
 * 偏移:
 *   0  magic        u16 le  = FSG_FRAME_MAGIC
 *   2  version      u8      = FSG_PROTO_VERSION
 *   3  hand         u8      (fsg_hand_t)
 *   4  seq          u32 le  逐帧自增，用于丢包检测
 *   8  timestamp_us u32 le  采集 MCU 采样时刻 (微秒，回绕约 71 分钟)
 *   12 node_count   u8      = FSG_NODE_COUNT
 *   13 odr_hz       u16 le  当前 ODR (100 / 200)
 *   15 flags        u8      预留
 */
typedef struct {
    uint16_t magic;
    uint8_t  version;
    uint8_t  hand;
    uint32_t seq;
    uint32_t timestamp_us;
    uint8_t  node_count;
    uint16_t odr_hz;
    uint8_t  flags;
} fsg_frame_header_t;          /* sizeof = 16 */

/* ---- 完整帧 ----
 * 布局: [header 16B][node_data 20B × FSG_NODE_COUNT][crc16 2B]
 * 固定长度 (16 节点): 16 + 320 + 2 = 338 字节
 * crc16 覆盖 header + 全部 node_data (不含 crc 自身)，CRC-16/CCITT-FALSE。
 */
typedef struct {
    fsg_frame_header_t header;
    fsg_node_data_t    nodes[FSG_NODE_COUNT];
    uint16_t           crc16;
} fsg_frame_t;
#pragma pack(pop)

#define FSG_FRAME_SIZE      ((uint16_t)sizeof(fsg_frame_t))   /* 338 */
#define FSG_FRAME_CRC_LEN   ((uint16_t)(sizeof(fsg_frame_t) - sizeof(uint16_t)))

/* ---- CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, 无反转) ---- */
static inline uint16_t fsg_crc16(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                                 : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/* 校验整帧 CRC 是否正确 */
static inline int fsg_frame_check(const fsg_frame_t *f)
{
    const uint8_t *p = (const uint8_t *)f;
    return fsg_crc16(p, FSG_FRAME_CRC_LEN) == f->crc16;
}

/* 计算并写入整帧 CRC（打包端调用） */
static inline void fsg_frame_finalize(fsg_frame_t *f)
{
    const uint8_t *p = (const uint8_t *)f;
    f->crc16 = fsg_crc16(p, FSG_FRAME_CRC_LEN);
}

#ifdef __cplusplus
}
#endif
#endif /* FSGLOVE_PROTOCOL_H */
