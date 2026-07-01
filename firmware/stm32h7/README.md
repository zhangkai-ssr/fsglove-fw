# FSGlove STM32H7 采集端固件

STM32H7 通过 2 条 SPI 总线读 16 个 9 轴 IMU 节点（LSM6DSOX + LIS3MDL），打时间戳/序号、
组帧（`fsg_frame_t`，338B），经 USART1 + DMA 发给 ESP32。

## 选型

| 用途 | 型号 | 说明 |
|---|---|---|
| **量产/自研板** | **STM32H750IBK6**（UFBGA176, 480MHz） | 项目实际用芯片；器件宏 `STM32H750xx` |
| 早期验证（可选） | Nucleo-H743ZI2 | 引脚兼容、板载 ST-Link，便于先跑通；仅 Flash 容量/型号宏不同 |

器件宏 `STM32H750xx`，CPU 480 MHz。

> ⚠ **H750 内部 Flash 仅 128KB**（H743 是 2MB）。本采集固件（HAL + App）体积小，
> 可放进 128KB（必要时 `CMAKE_BUILD_TYPE=Release` 即 `-O2`，或改 `-Os`）；
> 链接后看 `--print-memory-usage` 的 FLASH 占用。若将来固件超 128KB，需把代码放
> 外部 QSPI/OctoSPI 做 XIP 启动（另配 boot 引导 + 外部 flash 链接段）。
>
> ⚠ **HSE 时钟**：自研板若用外部晶振，改 `main.c` 的 `HSEState=RCC_HSE_ON`、按晶振频率调
> `PLLM`（使 PLL 输入 1~2MHz），并把 `hal_conf` 的 `HSE_VALUE` 改成实际值。当前默认是
> Nucleo 的 8MHz BYPASS。

## 这套工程是“可直接编译”的（不依赖 CubeMX）

构建系统用 **CMake + FetchContent**，首次 configure 会从 ST 官方仓库拉取
`cmsis_core` / `cmsis_device_h7` / `stm32h7xx_hal_driver`（CMSIS、startup、system、HAL 源码）。
设备相关的 **链接脚本、HAL 配置、中断、MSP、时钟/外设初始化** 都已在本目录写好。

### 依赖
- `arm-none-eabi-gcc`（建议 12+）、`cmake ≥ 3.20`、`ninja`
- `git` + 首次构建联网（FetchContent）

### 构建
```bash
cd firmware/stm32h7
cmake --preset stm32h7          # 首次会 clone CMSIS/HAL，稍慢
cmake --build --preset stm32h7
# 产物：build/fsglove_stm32h7.elf / .hex / .bin，并打印 size
```
> 若某 FetchContent tag 不存在，去对应 GitHub 仓库 Tags 页选一个，用
> `-DCMSIS_CORE_TAG=... -DCMSIS_DEVICE_TAG=... -DHAL_DRIVER_TAG=...` 覆盖。
> 也可改用本地已装的 STM32CubeH7（把 include 指到 `Drivers/CMSIS`、
> `Drivers/STM32H7xx_HAL_Driver`，并把 startup/system 加进源列表，见 CMakeLists 注释）。

### 烧录
```bash
# Nucleo: 拖 .bin 到 ST-Link 盘符，或：
STM32_Programmer_CLI -c port=SWD -w build/fsglove_stm32h7.bin 0x08000000 -rst
# 或: openocd -f board/st_nucleo_h743zi.cfg -c "program build/fsglove_stm32h7.elf verify reset exit"
```
烧录后 **PE1(LD2) 以 1Hz 闪烁** = 固件在跑；常亮 = 进了 `Error_Handler`。

## 引脚分配（Nucleo-H743ZI2，自研板按 PCB 改）

| 功能 | 引脚 | 备注 |
|---|---|---|
| SPI1 SCK/MISO/MOSI | PA5 / PA6 / PA7 (AF5) | 总线0 |
| SPI2 SCK/MISO/MOSI | PB13 / PB14 / PB15 (AF5) | 总线1；⚠ PB14=LD3，自研板可换 |
| USART1 TX/RX | PA9 / PA10 (AF7) | → ESP32（STM TX 接 ESP RX，必须共地） |
| USART1_TX DMA | DMA1_Stream0 | |
| 74HC154 #0（总线0 CS） | PA0..PA3=A0..3, PA4=E | E 低有效 |
| 74HC154 #1（总线1 CS） | PB0..PB3=A0..3, PB4=E | ⚠ PB3/PB4 默认 SWO/NJTRST，配 GPIO 后失效（SWD 不受影响） |
| 心跳/错误 LED | PE1 (LD2) | |

> SPI/UART 引脚在 `Core/Src/stm32h7xx_hal_msp.c`；LED/译码器引脚在 `Core/Src/main.c` 的
> `MX_GPIO_Init` 与 `App/cs_decoder.c`，改板时三处保持一致。

## 关键设计点

- **时钟 480MHz**：HSE BYPASS 8MHz（ST-Link MCO）→ PLL(M4/N480/P2) → CPU 480 / HCLK 240 / APB 120；VOS0 + FLASH_LATENCY_4。
- **SPI 内核时钟** = PLL1Q 120MHz，预分频 16 → SCLK ≈ 7.5MHz（≤10，可调 `BaudRatePrescaler`）。
- **DMA 不能访问 DTCM**：链接脚本把 `.data/.bss/堆/栈` 放 **AXI SRAM (0x24000000)**，且只开 **ICache、关 DCache**，UART-DMA 无需 cache 维护。
- **帧发送**：`esp_link_send_frame()` 用 `HAL_UART_Transmit_DMA` 一次发 338B；接收端靠 magic `0x4753`+定长+CRC16 自同步。
- **磁力计开关** `FSG_ENABLE_MAG`（`App/fsglove_config.h`）：置 0 退化纯 6 轴（LIS3MDL 已停产）。

## 还需按硬件补齐的 TODO

1. `App/fsglove_config.h` 的 `fsg_node_map[]`：按**实际 PCB 走线/74HC154 通道**逐行核对 bus/CS 地址。
2. 采样节拍：默认 SysTick 软节拍（抖动 ~1ms）；高精度改 TIM 周期中断（见 `acquisition.c` 注释）。
3. SPI DMA 双总线并行读（`spi_bus_transfer_dma`）目前是 TODO，当前为阻塞顺序读（200Hz 下占帧 ~6%，够用）。
4. 自研板：重映射 SPI2/译码器引脚，避开 LD3/JTAG。

## 文件
```
CMakeLists.txt / CMakePresets.json / cmake/gcc-arm-none-eabi.cmake   构建
STM32H743ZITX_FLASH.ld                                              链接脚本
Core/Inc/{main.h, stm32h7xx_hal_conf.h, stm32h7xx_it.h}
Core/Src/{main.c, stm32h7xx_hal_msp.c, stm32h7xx_it.c, syscalls.c, sysmem.c}
App/{acquisition,imu_node,spi_bus,cs_decoder,esp_link}.c + fsglove_config.h
Drivers/imu/{lsm6dsox,lis3mdl}.c
```

> 协议契约唯一权威：`firmware/common/fsglove_protocol.h`（App 层 `#include "../../common/fsglove_protocol.h"`）。

## 构建验证（已实测）

用 Arm GNU Toolchain 14.2.rel1 (gcc 14.2.1) + CMake 3.30.5 + Ninja 1.12.1，
`cmake --preset stm32h7 && cmake --build --preset stm32h7` **编译通过、零警告**，产物：
`fsglove_stm32h7.elf / .hex / .bin / .map`。实测占用（Debug `-Og`）：

| 区域 | 占用 | 容量 | 比例 |
|---|---|---|---|
| FLASH | 25,520 B | 128 KB | **19.5%**（H750 128KB 余量充足） |
| RAM_D1 (AXI SRAM) | 7,120 B | 512 KB | 1.4% |

> 依赖 tag：cmsis_core `v5.9.0` / cmsis_device_h7 `v1.10.7` / stm32h7xx_hal_driver `v1.11.6`
> （均由 FetchContent 自动拉取）。如换版本，按 CMakeLists 顶部 `*_TAG` 覆盖。
