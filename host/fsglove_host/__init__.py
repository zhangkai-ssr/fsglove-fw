"""FSGlove v2 上位机接收端。

经 UDP 接收 ESP32 转发的定长 IMU 数据帧，解析、（可选）姿态融合、记录。
协议契约见 firmware/common/fsglove_protocol.h（唯一权威）。
"""

from .protocol import (
    FRAME_SIZE,
    NODE_COUNT,
    MAGIC,
    PROTO_VERSION,
    NodeData,
    Frame,
    crc16,
    parse_frame,
    build_frame,
)

__all__ = [
    "FRAME_SIZE",
    "NODE_COUNT",
    "MAGIC",
    "PROTO_VERSION",
    "NodeData",
    "Frame",
    "crc16",
    "parse_frame",
    "build_frame",
]

__version__ = "0.1.0"
