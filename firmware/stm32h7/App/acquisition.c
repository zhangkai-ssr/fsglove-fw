/*
 * acquisition.c — 采集核心实现
 */
#include "acquisition.h"
#include "fsglove_config.h"
#include "imu_node.h"
#include "esp_link.h"
#include "main.h"

extern I2C_HandleTypeDef FSG_IMU_I2C;

static fsg_frame_t s_frame;          /* 单缓冲；双缓冲见下方 TODO */
static uint32_t    s_seq = 0;

/* ============================ micros() 时基 ============================ *
 * 用 Cortex-M7 的 DWT->CYCCNT 周期计数器换算微秒，分辨率高、无中断开销。
 * 回绕周期 = 2^32 / fCPU；与协议 timestamp_us(u32) 的 ~71 分钟回绕独立，
 * 这里输出 u32 微秒即可。
 */
static uint32_t s_cpu_mhz = 480u;    /* TODO: 用实际 SystemCoreClock/1e6 设置 */

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    s_cpu_mhz = SystemCoreClock / 1000000u;
    if (s_cpu_mhz == 0u) {
        s_cpu_mhz = 1u;
    }
}

uint32_t acquisition_micros(void)
{
    return DWT->CYCCNT / s_cpu_mhz;
}

void acquisition_init(void)
{
    dwt_init();

    imu_node_layer_init(&FSG_IMU_I2C);
    imu_node_probe();          /* 上电探测，置在线节点 */

    esp_link_init();

    /* 帧头中恒定字段只需填一次 */
    s_frame.header.magic      = FSG_FRAME_MAGIC;
    s_frame.header.version    = FSG_PROTO_VERSION;
    s_frame.header.hand       = (uint8_t)FSG_HAND;
    s_frame.header.node_count = FSG_NODE_COUNT;
    s_frame.header.odr_hz     = (uint16_t)FSG_ODR_HZ;
    s_frame.header.flags      = 0;
}

void acquisition_tick(void)
{
    /* 时间戳取本帧采集起点 */
    s_frame.header.timestamp_us = acquisition_micros();
    s_frame.header.seq          = s_seq++;

    /* 顺序读 16 节点。映射表已把节点分到 2 条总线，
     * 当前实现为阻塞顺序读；如需提升吞吐可两总线并行(DMA)。 TODO */
    for (uint8_t i = 0; i < FSG_NODE_COUNT; i++) {
        imu_node_read(i, &s_frame.nodes[i]);
    }

    /* 写 CRC（协议头提供，覆盖 header+nodes） */
    fsg_frame_finalize(&s_frame);

    /* 交链路发送。
     * 单缓冲下若上一帧 DMA 未完成，send 会返回忙；ODR<=200Hz、3M 波特率
     * 时 338B 发送 ~1.1ms，远小于 5ms 周期，单缓冲足够。
     * 若提高 ODR 或加重负载，改为 ping-pong 双缓冲。 TODO */
    esp_link_send_frame(&s_frame);
}

/* ============================ 调度说明 ============================ *
 * 固定频率节拍（二选一）：
 *
 * A) SysTick 计数法（main.c while(1) 内）：
 *      static uint32_t next = 0;
 *      uint32_t period_ms = 1000u / FSG_ODR_HZ;   // 100Hz->10ms, 200Hz->5ms
 *      if ((HAL_GetTick() - next) >= period_ms) { next += period_ms; acquisition_tick(); }
 *    简单但抖动 ~1ms（SysTick 粒度）。
 *
 * B) TIM 周期中断（推荐，抖动小）：
 *      CubeMX 配一个基本定时器，周期 = 1/FSG_ODR_HZ：
 *      在 HAL_TIM_PeriodElapsedCallback() 里置 flag，主循环检测 flag 调 tick；
 *      或直接在中断里 tick（注意中断内做 SPI 阻塞读不可取，宜置 flag）。
 *    TODO: 选定方案后在 main.c / TIM 回调接入。
 */
