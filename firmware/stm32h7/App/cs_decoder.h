/*
 * cs_decoder.h — 74HC154 (4->16) 片选译码器封装
 *
 * 每条 SPI 总线配 1 片 74HC154：4 根地址线 A0..A3 选 16 路输出，
 * 1 根使能 (E, 低有效) 控制是否选通。74HC154 输出低有效，
 * 正好作为 IMU 的 CS（低选通）。
 *
 * cs_decoder_select(bus, addr): 设地址 -> 拉低使能 -> 对应 CS 拉低
 * cs_decoder_deselect(bus)    : 拉高使能 -> 全部 CS 释放(高)
 */
#ifndef CS_DECODER_H
#define CS_DECODER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 选通某总线译码器的指定地址(0..15) */
void cs_decoder_select(uint8_t bus, uint8_t addr);

/* 释放某总线译码器（使能拉高，无 CS 被选） */
void cs_decoder_deselect(uint8_t bus);

#ifdef __cplusplus
}
#endif
#endif /* CS_DECODER_H */
