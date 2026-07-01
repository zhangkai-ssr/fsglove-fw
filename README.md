# FSGlove 固件/软件框架（v2 架构）

按 v2 硬件架构搭建的代码框架：**STM32H7（采集+打包）+ ESP32（Wi-Fi 回传）+ 上位机（融合/记录）**。
本仓库是**可扩展骨架**：协议契约、模块划分、关键驱动寄存器与数据流已就位；
硬件相关的时钟/引脚初始化（CubeMX 生成）、具体 PCB 引脚映射等以 `TODO` 标出。

## 数据流

```text
16 个 9 轴节点 (LSM6DSOXTR + LIS3MDLTR)
  · 每指 1 条 FPC、每条 3 节点 + 手背参考 1 = 16
  · 2 条 SPI 总线，每条 8 节点，CS 经 74HC154/138 译码
  -> STM32H7 (480MHz M7)：DMA 并行读 -> 打时间戳/seq -> 组 fsg_frame_t (338B)
  -> 内部链路 (UART 默认 / 可换 SPI)
  -> ESP32：Wi-Fi STA + UDP，原样转发整帧
  -> 上位机 (Python)：解析 -> 姿态融合 (Mahony 占位) -> 记录
```

## 目录结构

```text
firmware/
  common/
    fsglove_protocol.h     # ★三端共享的线缆协议契约（帧格式 + CRC16），改动需三端同步
  stm32h7/                 # 采集端固件 (STM32 HAL)
    Core/                  # main / 时钟 / 中断 (CubeMX 生成位)
    Drivers/imu/           # lsm6dsox / lis3mdl 寄存器与读写
    App/                   # spi_bus / cs_decoder / imu_node / acquisition / esp_link / config
    README.md
  esp32/                   # 回传端固件 (ESP-IDF + FreeRTOS)
    main/                  # stm_link / wifi_sta / net_sender / app_main
    README.md
host/                      # 上位机 (Python)
  fsglove_host/            # protocol / receiver / fusion / recorder
  README.md
tools/                     # 辅助脚本（协议自测等）
```

## 关键设计约定

- **协议契约唯一来源**：`firmware/common/fsglove_protocol.h`。STM32 打包、ESP32 转发、Python 解析都以它为准；上位机 `protocol.py` 是它的镜像，二者改动必须同步。
- **整帧定长 338 字节**（16 节点）：帧头 16B + 16×20B 节点 + CRC16 2B，全小端，CRC-16/CCITT-FALSE。
- **内部链路抽象**：STM32 侧 `esp_link.*`、ESP32 侧 `stm_link.*` 默认 UART 实现，接口与帧同步逻辑与物理层解耦，后续换 SPI 只替换底层收发。
- **采样率**：默认 100 Hz（首版），可配 200 Hz；SPI 链路余量极大。
- **可替换点**：UART↔SPI 内部链路、UDP↔TCP 回传、Python↔C++ 上位机，均已隔离在各自模块。

## ⚠️ 物料风险（已在硬件文档记录）

磁力计 **LIS3MDLTR 已被 ST 停产（NRND）**，无同级替代。框架在 `lis3mdl` 驱动与 `imu_node` 中把磁力计读取做成**可编译开关**（`FSG_ENABLE_MAG`），便于后续切到纯 6 轴或更换磁力计型号。

## 构建

- STM32H7：**CMake + FetchContent，可直接编译**（不需 CubeMX）。
  `cd firmware/stm32h7 && cmake --preset stm32h7 && cmake --build --preset stm32h7`
  目标板 Nucleo-H743ZI2（STM32H743ZI）/ 自研板 STM32H743VIT6。详见其 README。
- ESP32：见 `firmware/esp32/README.md`（`idf.py build flash monitor`）。
- 上位机：见 `host/README.md`（`pip install -r requirements.txt && python -m fsglove_host.main`）。

> 上位机协议自测已通过（`tools/proto_selftest.py`）。STM32/ESP32 固件需在装有
> arm-none-eabi-gcc / ESP-IDF 的机器上构建；PCB 引脚映射与节点片选表见各端 README 的 TODO。
