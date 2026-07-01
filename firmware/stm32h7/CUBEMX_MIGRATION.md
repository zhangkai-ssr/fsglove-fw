# CubeMX 迁移指南（方案 2：让 CubeMX 接管 Core 初始化）

本工程原本是「手写、不依赖 CubeMX、已编译验证」的固件。本指南把硬件初始化
（时钟/GPIO/SPI/USART/DMA/TIM/NVIC）交给 `fsglove_stm32h7.ioc` 管理，**App 层
（`App/*`、`Drivers/imu/*`、`common/fsglove_protocol.h`）完全不动**。

> 目标：以后改引脚/加外设 = 在 CubeMX 点一点重生成，单一真值源，不再三处手改。
> 代价：首次迁移要把「480MHz 时钟 / DCache 关 / 数据放 AXI-SRAM / App 接线」保住。

---

## 0. `.ioc` 里已配好的东西（与现有固件逐字对齐）

| 外设 | 配置 | 对应现有代码 |
|---|---|---|
| MCU | STM32H750IBKx, UFBGA176 | README / CMakeLists |
| 时钟 | HSE BYPASS 8MHz → PLL(M4/N480/P2/Q8) → SYSCLK 480 / HCLK 240 / APB 120；VOS0 | `main.c` SystemClock_Config |
| SPI123 内核时钟 | PLL1Q = 120MHz | `main.c` periph.Spi123ClockSelection |
| SPI1 | Full-Duplex Master, 8bit, MSB, **CPOL=High/CPHA=2Edge(Mode3)**, **NSS=Soft**, 预分频16≈7.5MHz | `spi_common_init` |
| SPI2 | 同 SPI1 | `spi_common_init` |
| USART1 | 3 000 000 / 8N1 / TX+RX | `MX_USART1_UART_Init` |
| GPIO 译码器 | `DEC0_A0..A3=PA0..3`,`DEC0_E=PA4`；`DEC1_A0..A3=PB0..3`,`DEC1_E=PB4`；E 初始高 | `cs_decoder.c` / `MX_GPIO_Init` |
| 心跳 LED | `LED_HB=PE1` | `main.c` LED_PORT/PIN |
| ESP 控制(预留) | `ESP_EN=PD3`,`ESP_BOOT=PD4` | 规格书 §5.3（现固件未用，占位） |
| Cache | **ICache 开 / DCache 关** | `main.c` SCB_EnableICache |
| DMA | SPI1_RX/TX、SPI2_RX/TX、USART1_TX | USART1_TX 现已用；SPI-DMA 为架构预留 |
| TIM6 | 100Hz 周期中断（节拍，预分频999/ARR2399 @240MHz）| `acquisition.c` 推荐方案 B |

> ⚠ 引脚目前沿用 Nucleo-H743ZI2 布局（README）。自研 H750IBK6 板按实际 PCB 在
> CubeMX 里拖引脚即可，**改完只动 `.ioc`，标签名 `DEC0_*/DEC1_*/LED_HB` 保持不变**，
> 这样 `cs_decoder.c` 的宏不用改。

---

## 1. 打开与设置（一次性）

1. 双击 `fsglove_stm32h7.ioc`，CubeMX 若提示迁移 FW 版本，点 **Migrate**。
2. **Clock Configuration** 页核对（最易因版本漂移，务必看）：
   - Input frequency = **8** MHz，HSE = **BYPASS**
   - `/M=4` `×N=480` `/P=2` → **HCLK 240**、**System Clock 480 MHz**
   - `/Q1=8` → 120MHz；SPI123 mux 选 **PLL1Q**
   - Voltage Scaling = **VOS0**，Flash Latency 自动（应为 4WS）
   - 若数值不对，手动把上面填对，直到顶部显示 480MHz 不报红。
3. **Project Manager → Code Generator**：
   - 勾 **Generate peripheral initialization as a pair of '.c/.h'** → **取消勾选**
     （保持初始化写在 `main.c`，与现有结构一致）
   - 勾 **Keep User Code when re-generating**
   - **不要**勾 "Delete previously generated files..."（避免误删 App/Drivers）

---

## 2. 生成代码——用 scratch 目录，只回拷 Core（**关键，防止覆盖构建系统**）

现有 `CMakeLists.txt / cmake/ / CMakePresets.json / STM32H750IBKx_FLASH.ld` 是**手写定制**的
（FetchContent 拉 HAL、AXI-SRAM 链接段）。CubeMX 选 CMake 工具链会生成它自己那一套并**覆盖**这些文件。
因此**不要原地 Generate**，按下面隔离生成：

1. 把 `.ioc` 的 **Project Manager → Project Location** 临时指向一个空目录，如 `D:\fsglove_cubemx_scratch\`，Project Name 任意。
2. **GENERATE CODE**。
3. 只把下列文件**回拷**到本工程，覆盖同名：
   ```
   scratch/Core/Inc/*.h                → firmware/stm32h7/Core/Inc/
   scratch/Core/Src/main.c             → firmware/stm32h7/Core/Src/
   scratch/Core/Src/stm32h7xx_hal_msp.c→ firmware/stm32h7/Core/Src/
   scratch/Core/Src/stm32h7xx_it.c     → firmware/stm32h7/Core/Src/
   ```
   **不要**回拷：`CMakeLists.txt`、`cmake/`、`*.ld`、`startup_*.s`、`system_*.c`、
   `syscalls.c`、`sysmem.c`（这些保留本工程现有版本）。

> 本工程 `CMakeLists.txt` 是显式列源文件、不 glob，且 startup/system 走 FetchContent、
> linker 用自带 `STM32H750IBKx_FLASH.ld`。所以只要 Core 那 3 个 .c + Inc 头进来即可，
> CubeMX 多生成的 startup/system/linker 不进编译、零影响。

---

## 3. 把 App 接线塞回生成的 `main.c`（USER CODE 区，重生成不丢）

CubeMX 生成的 `main()` 句柄名正好是 `hspi1/hspi2/huart1`，与 `main.h` 别名一致，无需改 App。
在生成的 `Core/Src/main.c` 里按区块补：

```c
/* USER CODE BEGIN Includes */
#include "fsglove_config.h"
#include "acquisition.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2  —— 在 MX_*_Init() 全部调用之后 */
    acquisition_init();                 /* DWT 时基 + 绑定总线 + 节点 probe + 链路 */
    HAL_TIM_Base_Start_IT(&htim6);      /* 启动 100Hz 采样节拍（方案 B） */
/* USER CODE END 2 */

/* USER CODE BEGIN PV  —— 采样标志 */
static volatile uint8_t s_tick = 0;
/* USER CODE END PV */

/* USER CODE BEGIN 4  —— TIM6 周期回调置标志 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) s_tick = 1;
}
/* USER CODE END 4 */

/* USER CODE BEGIN WHILE  —— 主循环 */
  while (1)
  {
    if (s_tick) { s_tick = 0; acquisition_tick(); }
    /* 心跳：用 HAL_GetTick 翻转 LED_HB(PE1) */
    static uint32_t t = 0;
    if (HAL_GetTick() - t >= 500u) { t = HAL_GetTick(); HAL_GPIO_TogglePin(LED_HB_GPIO_Port, LED_HB_Pin); }
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
  }
```

> 若想先保持原来的 SysTick 软节拍（不依赖 TIM6），把 `HAL_TIM_Base_Start_IT` 与回调去掉，
> 在 while 里用 `HAL_GetTick()` 比较 `period_ms` 调 `acquisition_tick()`（同现 `main.c:50`）。
> TIM6 仍配着不碍事。

---

## 4. 保住三个「不能被 CubeMX 默认值冲掉」的设计

1. **DCache 关**：`.ioc` 已设 CORTEX_M7 → ICache=Enabled, DCache=Disabled，生成的 `main.c`
   只会 `SCB_EnableICache()`。**确认没有 `SCB_EnableDCache()`**。否则 UART/SPI 的 DMA 缓冲
   要做 cache 维护，现有代码没做 → 会偶发坏帧。
2. **数据放 AXI-SRAM**：继续用本工程 `STM32H750IBKx_FLASH.ld`（.data/.bss/堆/栈 在
   `0x24000000` AXI SRAM），**不要**用 CubeMX 默认 linker（它把 .data 放 DTCM，DMA 访问不到）。
   —— 第 2 步「不回拷 *.ld」已保证。
3. **HSE 来源**：现 `.ioc` = BYPASS 8MHz（Nucleo ST-Link MCO）。**自研板若用晶振**，
   在 CubeMX 把 PH0/PH1 改成 Crystal、按晶振频率调 M 使 PLL 输入 ∈[1,2]MHz，并同步改
   `Core/Inc/stm32h7xx_hal_conf.h` 的 `HSE_VALUE`。

---

## 5. 构建与对平（迁移验收，逐项打勾）

```bash
cd firmware/stm32h7
cmake --preset stm32h7
cmake --build --preset stm32h7
```

- [ ] 编译**零警告**通过（同迁移前 README 实测）
- [ ] `--print-memory-usage`：FLASH 占用与迁移前相近（约 20% / 128KB）
- [ ] 烧录后 **PE1 以 1Hz 闪烁**（固件在跑，没进 Error_Handler）
- [ ] 示波器：SPI1/SPI2 SCLK ≈ **7.5 MHz**；译码器 E 默认高、选中时对应 CS 拉低
- [ ] 抓 USART1 TX：周期性 **338B** 整帧，帧头 `0x53 0x47`(magic 0x4753)
- [ ] 采样节拍周期 = `1000/FSG_ODR_HZ` ms（100Hz→10ms / 200Hz→5ms）

任一项不对，回看第 1 步（时钟）或第 4 步（cache/linker）。

---

## 6. 之后的日常（这就是方案 2 的收益）

- 改引脚 / 加外设 → 开 `.ioc`，拖引脚或勾外设 → 重复第 2~3 步（scratch 生成 + 回拷 Core + USER CODE 已自动保留）。
- 译码器/总线映射变了 → 只改 `App/fsglove_config.h` 的 `fsg_node_map[]`，CubeMX 不碰。
- 标签名（`DEC0_*`/`DEC1_*`/`LED_HB`）保持不变，`cs_decoder.c`/`main.c` 引用即稳定。
