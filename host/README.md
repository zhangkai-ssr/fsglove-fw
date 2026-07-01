# FSGlove v2 上位机接收端（Python）

经 UDP 接收 ESP32 转发的定长 IMU 数据帧，解析、（可选）姿态融合、记录。

数据流：
```
16×9轴节点 -> STM32H7(采集/打包) -> 内部链路 -> ESP32(Wi-Fi UDP 转发) -> 本上位机
```

## 安装与运行

```bash
pip install -r requirements.txt

# 接收 + 融合 + 记录 CSV
python -m fsglove_host.main --fuse --record csv --out run.csv

# 仅接收并打印帧率/丢帧（默认端口 18888）
python -m fsglove_host.main

# 落原始二进制（每帧 338B）
python -m fsglove_host.main --record bin --out run.bin
```

CLI 参数：

| 参数 | 说明 |
| --- | --- |
| `--bind` | 绑定 IP，默认 `0.0.0.0` |
| `--port` | UDP 端口，默认 `18888` |
| `--record` | `csv` / `bin` / `none`（默认 none） |
| `--out` | 记录输出路径 |
| `--fuse` | 启用 Mahony 姿态融合（占位） |
| `--print` | 状态打印频率 Hz，默认 1.0，0 关闭 |

## 与协议头的对应关系

唯一权威：`../firmware/common/fsglove_protocol.h`。Python 解析逐字段与之一致。

帧布局（全小端，1 字节对齐）：`[帧头 16B][节点 20B × 16][crc16 2B]` = **338B 定长**。

帧头（16B）：

| 偏移 | 字段 | 类型 | 含义 |
| --- | --- | --- | --- |
| 0 | magic | u16 | `0x4753`（'S''G'） |
| 2 | version | u8 | 协议版本 `0x02` |
| 3 | hand | u8 | 0=左 1=右 |
| 4 | seq | u32 | 逐帧自增，丢包检测 |
| 8 | timestamp_us | u32 | 采样时刻（微秒，约 71 分回绕） |
| 12 | node_count | u8 | `16` |
| 13 | odr_hz | u16 | ODR（100/200） |
| 15 | flags | u8 | 预留 |

节点（20B）：

| 偏移 | 字段 | 类型 |
| --- | --- | --- |
| 0 | node_id | u8 |
| 1 | status | u8（位见下） |
| 2 | accel[3] | i16×3 |
| 8 | gyro[3] | i16×3 |
| 14 | mag[3] | i16×3 |

status 位：`ACCEL_OK=1<<0`、`GYRO_OK=1<<1`、`MAG_OK=1<<2`、`ONLINE=1<<3`、`FRESH=1<<4`。

CRC：**CRC-16/CCITT-FALSE**（poly `0x1021`，init `0xFFFF`，输入/输出均不反转），覆盖前 **336B**（整帧去掉末尾 crc）。

CSV 列：`seq,timestamp_us,node_id,ax,ay,az,gx,gy,gz,mx,my,mz,status`（每节点每帧一行）。

## 模块

- `protocol.py` — struct 格式串、`NodeData` / `Frame`、`crc16`、`parse_frame`、`build_frame`、常量。
- `receiver.py` — `UdpReceiver`：`recv_frames()` 生成器，含收包/CRC失败/丢帧统计。
- `fusion.py` — `MahonyAHRS` + `FusionBank`（占位融合，用 numpy）。
- `recorder.py` — `Recorder`：CSV / 原始 `.bin`，上下文管理器。
- `main.py` — CLI 入口。

## 把 UDP 改成 TCP

接收逻辑集中在 `receiver.py`，解析层 `parse_frame` 与传输无关，迁移只动接收层：

1. 建 `socket.SOCK_STREAM`，`listen()` + `accept()` 得到连接 socket。
2. TCP 是字节流而非数据报，**必须自己按 338B 拆帧**：循环 `recv()` 累积到缓冲区，
   每凑满 `FRAME_SIZE` 字节就切出一帧交给 `parse_frame`，剩余字节留作下帧。
   （可加 magic 重同步：解析失败时按字节滑动直到对齐 `0x4753`。）
3. 其余统计/生成器接口保持不变。

## 关于姿态融合（占位，需后续替换）

`fusion.py` 的 Mahony 实现仅为打通"原始计数 -> 四元数"链路的**最小占位**，
落地前需补：

- **单位换算**：accel/gyro/mag 当前是 i16 原始计数，未按灵敏度换算；
  gyro 必须转 rad/s（`MahonyAHRS(gyro_scale=...)`），否则积分错误。
- **传感器标定**：陀螺零偏、加计/磁力计椭球标定、磁硬铁/软铁补偿。
- **坐标系对齐**：各节点安装姿态到统一手部坐标系的外参。
- 可替换为成熟库（如 `ahrs`）或 Madgwick / EKF。
