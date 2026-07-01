/*
 * cs_decoder.c — 74HC154 译码器 GPIO 操作实现
 *
 * 引脚定义全部为 TODO：须按 CubeMX 中分配的 GPIO 端口/引脚填入。
 * 每条总线 5 根线：A0..A3(地址) + E(使能, 低有效)。
 */
#include "cs_decoder.h"
#include "fsglove_config.h"

/* CubeMX 生成的 HAL 头（main.h 汇总句柄/引脚宏） */
#include "main.h"

/* ============================ 引脚映射 TODO ============================ *
 * 下列宏须替换为 CubeMX 实际生成的 GPIOx / GPIO_PIN_y 宏。
 * 命名建议在 CubeMX 中用 User Label，如 DEC0_A0 / DEC0_E ...
 */
/* --- 总线 0 译码器 --- */
#define DEC0_A0_PORT   GPIOA   /* TODO */
#define DEC0_A0_PIN    GPIO_PIN_0   /* TODO */
#define DEC0_A1_PORT   GPIOA   /* TODO */
#define DEC0_A1_PIN    GPIO_PIN_1   /* TODO */
#define DEC0_A2_PORT   GPIOA   /* TODO */
#define DEC0_A2_PIN    GPIO_PIN_2   /* TODO */
#define DEC0_A3_PORT   GPIOA   /* TODO */
#define DEC0_A3_PIN    GPIO_PIN_3   /* TODO */
#define DEC0_E_PORT    GPIOA   /* TODO 使能, 低有效 */
#define DEC0_E_PIN     GPIO_PIN_4   /* TODO */

/* --- 总线 1 译码器 --- */
#define DEC1_A0_PORT   GPIOB   /* TODO */
#define DEC1_A0_PIN    GPIO_PIN_0   /* TODO */
#define DEC1_A1_PORT   GPIOB   /* TODO */
#define DEC1_A1_PIN    GPIO_PIN_1   /* TODO */
#define DEC1_A2_PORT   GPIOB   /* TODO */
#define DEC1_A2_PIN    GPIO_PIN_2   /* TODO */
#define DEC1_A3_PORT   GPIOB   /* TODO */
#define DEC1_A3_PIN    GPIO_PIN_3   /* TODO */
#define DEC1_E_PORT    GPIOB   /* TODO 使能, 低有效 */
#define DEC1_E_PIN     GPIO_PIN_4   /* TODO */

/* 单根地址线描述 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} cs_pin_t;

typedef struct {
    cs_pin_t a[4];   /* A0..A3 */
    cs_pin_t e;      /* 使能, 低有效 */
} cs_decoder_t;

static const cs_decoder_t s_decoders[FSG_SPI_BUS_COUNT] = {
    /* bus 0 */
    { { { DEC0_A0_PORT, DEC0_A0_PIN }, { DEC0_A1_PORT, DEC0_A1_PIN },
        { DEC0_A2_PORT, DEC0_A2_PIN }, { DEC0_A3_PORT, DEC0_A3_PIN } },
      { DEC0_E_PORT, DEC0_E_PIN } },
    /* bus 1 */
    { { { DEC1_A0_PORT, DEC1_A0_PIN }, { DEC1_A1_PORT, DEC1_A1_PIN },
        { DEC1_A2_PORT, DEC1_A2_PIN }, { DEC1_A3_PORT, DEC1_A3_PIN } },
      { DEC1_E_PORT, DEC1_E_PIN } },
};

void cs_decoder_select(uint8_t bus, uint8_t addr)
{
    if (bus >= FSG_SPI_BUS_COUNT) {
        return;
    }
    const cs_decoder_t *d = &s_decoders[bus];

    /* 先确保未选通，避免切换地址时误触发其它 CS */
    HAL_GPIO_WritePin(d->e.port, d->e.pin, GPIO_PIN_SET);   /* E=高: 禁用 */

    /* 设 4bit 地址 */
    for (int i = 0; i < 4; i++) {
        GPIO_PinState st = ((addr >> i) & 0x1u) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        HAL_GPIO_WritePin(d->a[i].port, d->a[i].pin, st);
    }

    /* 拉低使能 -> 对应输出(=CS)被拉低，选中目标传感器 */
    HAL_GPIO_WritePin(d->e.port, d->e.pin, GPIO_PIN_RESET);  /* E=低: 选通 */
}

void cs_decoder_deselect(uint8_t bus)
{
    if (bus >= FSG_SPI_BUS_COUNT) {
        return;
    }
    /* 使能拉高，16 路输出全部回到高电平，无 CS 选中 */
    HAL_GPIO_WritePin(s_decoders[bus].e.port, s_decoders[bus].e.pin, GPIO_PIN_SET);
}
