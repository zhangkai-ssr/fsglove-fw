/*
 * stm32h7xx_it.h — 中断服务程序声明
 */
#ifndef STM32H7xx_IT_H
#define STM32H7xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Cortex-M7 系统异常 */
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

/* 外设中断 */
void DMA1_Stream0_IRQHandler(void);   /* USART1_TX DMA */
void USART1_IRQHandler(void);

#ifdef __cplusplus
}
#endif
#endif /* STM32H7xx_IT_H */
