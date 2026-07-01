# FSGlove v2 — ESP32 Wi-Fi 回传端固件

ESP32 作为无线协处理器：从 STM32H7 经 UART 收定长数据帧 → 帧同步 + CRC 校验 →
经 Wi-Fi(STA) 用 UDP 转发给上位机。ESP32 **不参与采集**，只做透传与校验。

```
[16 节点] -> STM32H7 (采集/打包 fsg_frame_t) --UART 3Mbps--> ESP32 --Wi-Fi/UDP--> 上位机
```

## 目录结构

```
esp32/
├── CMakeLists.txt          # 工程根 CMake
├── sdkconfig.defaults      # 默认 sdkconfig
├── README.md
└── main/
    ├── CMakeLists.txt      # 组件注册（含 ../../common 头路径）
    ├── fsg_config.h        # Wi-Fi/上位机/UART 等编译期配置
    ├── app_main.c          # 入口：NVS -> Wi-Fi -> UDP -> Queue -> 主循环转发
    ├── stm_link.{h,c}      # UART 接收 + 帧同步（magic+定长+CRC）-> Queue
    ├── wifi_sta.{h,c}      # STA 连接 / 断线自动重连
    └── net_sender.{h,c}    # UDP socket 发送整帧
```

## 与共享协议头的关系

帧格式由唯一权威头文件定义：`../common/fsglove_protocol.h`（三端共享）。
本工程 **不复制** 该结构体，而是通过 `main/CMakeLists.txt` 里的
`INCLUDE_DIRS "../../common"` 直接 `#include "fsglove_protocol.h"`，
使用其中的 `fsg_frame_t`、`FSG_FRAME_SIZE`(338B)、`FSG_FRAME_MAGIC`、
`fsg_frame_check()`。STM32 端、上位机端必须使用同一头文件，保证一致。

## 构建 / 烧录 / 监视

需先安装 ESP-IDF（建议 v5.x）并 `. $IDF_PATH/export.sh`（Windows 用
`export.bat` 或 ESP-IDF PowerShell）。

```bash
idf.py set-target esp32      # 选目标芯片（如 esp32 / esp32s3）
idf.py menuconfig            # 可选：调参数
idf.py build
idf.py -p <PORT> flash       # PORT 如 COM5 / /dev/ttyUSB0
idf.py -p <PORT> monitor     # 看日志（Ctrl+] 退出）
```

## 配置 Wi-Fi 与上位机 IP

改 `main/fsg_config.h`：

| 宏 | 含义 | 默认 |
|----|------|------|
| `FSG_WIFI_SSID` / `FSG_WIFI_PASS` | 要连的 AP | 占位，**必须改** |
| `FSG_HOST_IP` | 上位机 IP | `192.168.1.100` |
| `FSG_HOST_PORT` | 上位机 UDP 端口 | `18888` |

> 这些项目前用 `#define`。若想免改源码，可改为 menuconfig：在
> `main/Kconfig.projbuild` 声明选项，再用 `CONFIG_xxx` 替换对应宏。

## UART 接线（与 STM32 交叉）

| ESP32 | 方向 | STM32H7 |
|-------|------|---------|
| `FSG_UART_TX_PIN` (默认 GPIO17) | -> | STM32 USART RX |
| `FSG_UART_RX_PIN` (默认 GPIO16) | <- | STM32 USART TX |
| GND | --- | GND（必须共地） |

波特率默认 **3000000**，两端必须一致。引脚/端口在 `fsg_config.h` 修改。
3Mbps 下务必保证 RX 环形缓冲足够（`FSG_UART_RX_BUF_SIZE`，默认 16KB）。

## 帧同步说明

`stm_link.c` 对 UART 字节流做软件帧同步：

1. 在缓冲中找 2 字节 magic（`FSG_FRAME_MAGIC` 小端 = `0x53 0x47`）。
2. 收满 `FSG_FRAME_SIZE` (338) 字节。
3. `fsg_frame_check()` 验 CRC-16/CCITT-FALSE。
   - 通过：整帧投递队列，缓冲前移一帧；
   - 失败：**滑动 1 字节**重新找 magic，从而在错位/丢字节后快速重新对齐。

完整有效帧通过 `QueueHandle_t`（元素 `fsg_frame_t`）交给主循环转发。
**低延迟优先**：队列满时丢弃新帧（不阻塞 UART 任务）；丢帧可接受，上位机
依据帧头 `seq` 检测丢包。主循环每秒打印 `seq / fps / 累计丢帧 / wifi 状态`。

## 可选：把 UDP 改成 TCP

UDP 低延迟、丢帧可接受，是默认方案。若上位机网络丢包严重需要可靠传输，
改动集中在 `main/net_sender.c`：

- `socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)` -> `SOCK_STREAM, IPPROTO_TCP`；
- `net_sender_init()` 里 socket 之后加 `connect(s_sock, &s_dest, ...)`；
- `net_sender_send()` 里把 `sendto(...)` 换成 `send(s_sock, frame, FSG_FRAME_SIZE, 0)`；
- 处理 TCP 断连：`send` 失败时关闭并在 Wi-Fi 在线时重新 `connect`。

注意 TCP 是字节流，上位机需自行按 `FSG_FRAME_SIZE` 重新分帧（或复用本端
同样的 magic+CRC 同步逻辑）。
