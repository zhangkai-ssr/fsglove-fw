/*
 * STM32H750IBKx_FLASH.ld — STM32H750IBK6 链接脚本（在内部 FLASH 运行）
 *
 * ⚠ H750 内部 FLASH 只有 **128KB** @0x08000000（不是 H743 的 2MB）。
 *   本采集固件（HAL + App）可放下；若将来固件超 128KB，需把代码放外部
 *   QSPI/OctoSPI 并改 XIP 启动（另配 boot + 外部 flash 链接段）。
 *
 * ⚠ .data/.bss/堆/栈放 **AXI SRAM (D1, 0x24000000, 512KB)** 而非 DTCMRAM。
 *   原因：H7 的 DMA1/DMA2 引擎**无法访问 DTCM/ITCM**（TCM 仅 CPU 可达）。
 *   采集帧缓冲 s_frame 要通过 USART1 DMA 发出，必须落在 DMA 可达内存（AXI SRAM）。
 *   配合固件中只开 ICache、关 DCache，DMA 无需做 cache 维护即可正确收发。
 *   RAM 与 H743 相同。
 */

ENTRY(Reset_Handler)

/* 栈顶 = AXI SRAM 末尾；堆/栈最小尺寸 */
_estack    = ORIGIN(RAM_D1) + LENGTH(RAM_D1);
_Min_Heap_Size  = 0x800;   /* 2 KB  */
_Min_Stack_Size = 0x1000;  /* 4 KB  */

MEMORY
{
  DTCMRAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  ITCMRAM (xrw) : ORIGIN = 0x00000000, LENGTH = 64K
  FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
  RAM_D1  (xrw) : ORIGIN = 0x24000000, LENGTH = 512K
  RAM_D2  (xrw) : ORIGIN = 0x30000000, LENGTH = 288K
  RAM_D3  (xrw) : ORIGIN = 0x38000000, LENGTH = 64K
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* 初始化数据：LMA 在 FLASH，运行期在 AXI SRAM (DMA 可达) */
  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM_D1 AT> FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM_D1

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM_D1

  /DISCARD/ : { libc.a ( * ) libm.a ( * ) libgcc.a ( * ) }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
