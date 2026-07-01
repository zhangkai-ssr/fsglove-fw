/*
 * stm32h7xx_it.c — 中断服务程序
 */
#include "main.h"
#include "stm32h7xx_it.h"

/* main.c / hal_msp.c 中定义的句柄 */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_tx;

/* ===================== Cortex-M7 系统异常 ===================== */
void NMI_Handler(void)        { while (1) {} }
void HardFault_Handler(void)  { while (1) {} }
void MemManage_Handler(void)  { while (1) {} }
void BusFault_Handler(void)   { while (1) {} }
void UsageFault_Handler(void) { while (1) {} }
void SVC_Handler(void)        {}
void DebugMon_Handler(void)   {}
void PendSV_Handler(void)     {}

void SysTick_Handler(void)
{
    HAL_IncTick();   /* HAL_GetTick() / 超时计时所需 */
}

/* ===================== 外设中断 ===================== */
void DMA1_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
