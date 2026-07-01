"""FSGlove v2 线缆协议解析（与 firmware/common/fsglove_protocol.h 逐字段一致）。

帧布局: [帧头 16B][节点 20B × 16][crc16 2B] = 338B 定长，全小端。
帧头偏移:  magic u16@0, version u8@2, hand u8@3, seq u32@4,
           timestamp_us u32@8, node_count u8@12, odr_hz u16@13, flags u8@15
节点偏移:  node_id u8@0, status u8@1, accel[3] i16@2, gyro[3] i16@8, mag[3] i16@14
CRC:       CRC-16/CCITT-FALSE(poly 0x1021, init 0xFFFF, 不反转)，覆盖前 336B。
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import List, Optional

# ---- 规模/魔数常量（必须与 C 头一致）----
PROTO_VERSION = 0x02
MAGIC = 0x4753          # 'S''G' 小端
NODE_COUNT = 16
HEADER_SIZE = 16
NODE_SIZE = 20
CRC_SIZE = 2
FRAME_SIZE = HEADER_SIZE + NODE_SIZE * NODE_COUNT + CRC_SIZE   # 338
FRAME_CRC_LEN = FRAME_SIZE - CRC_SIZE                          # 336，CRC 覆盖范围

# ---- 状态位（status 字段）----
ST_ACCEL_OK = 1 << 0
ST_GYRO_OK = 1 << 1
ST_MAG_OK = 1 << 2
ST_NODE_ONLINE = 1 << 3
ST_DATA_FRESH = 1 << 4

# ---- struct 格式字符串（'<' = 小端）----
# 帧头 16B: magic u16, version u8, hand u8, seq u32, timestamp_us u32,
#           node_count u8, odr_hz u16, flags u8
_HEADER_FMT = "<HBBIIBHB"
# 节点 20B: node_id u8, status u8, accel[3] i16, gyro[3] i16, mag[3] i16
_NODE_FMT = "<BB3h3h3h"

_HEADER = struct.Struct(_HEADER_FMT)
_NODE = struct.Struct(_NODE_FMT)
_CRC = struct.Struct("<H")

# 编译期断言：格式字符串长度须与协议一致
assert _HEADER.size == HEADER_SIZE, "帧头格式与协议不符"
assert _NODE.size == NODE_SIZE, "节点格式与协议不符"


@dataclass
class NodeData:
    """单节点原始 9 轴数据。"""

    node_id: int
    status: int
    accel: tuple          # (ax, ay, az) i16
    gyro: tuple           # (gx, gy, gz) i16
    mag: tuple            # (mx, my, mz) i16

    @property
    def accel_ok(self) -> bool:
        return bool(self.status & ST_ACCEL_OK)

    @property
    def gyro_ok(self) -> bool:
        return bool(self.status & ST_GYRO_OK)

    @property
    def mag_ok(self) -> bool:
        return bool(self.status & ST_MAG_OK)

    @property
    def online(self) -> bool:
        return bool(self.status & ST_NODE_ONLINE)

    @property
    def fresh(self) -> bool:
        return bool(self.status & ST_DATA_FRESH)


@dataclass
class Frame:
    """完整一帧解析结果。"""

    version: int
    hand: int
    seq: int
    timestamp_us: int
    odr_hz: int
    nodes: List[NodeData]
    flags: int = 0
    node_count: int = NODE_COUNT


def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, 无输入/输出反转。"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def parse_frame(buf: bytes) -> Optional[Frame]:
    """解析一帧。长度/magic/CRC 任一不符返回 None。"""
    if len(buf) != FRAME_SIZE:
        return None

    (magic, version, hand, seq, timestamp_us,
     node_count, odr_hz, flags) = _HEADER.unpack_from(buf, 0)
    if magic != MAGIC:
        return None

    # CRC 覆盖前 336B（整帧去掉末尾 crc），与发送端一致
    (crc_recv,) = _CRC.unpack_from(buf, FRAME_CRC_LEN)
    if crc16(buf[:FRAME_CRC_LEN]) != crc_recv:
        return None

    nodes: List[NodeData] = []
    off = HEADER_SIZE
    for _ in range(NODE_COUNT):
        (node_id, status,
         ax, ay, az, gx, gy, gz, mx, my, mz) = _NODE.unpack_from(buf, off)
        nodes.append(NodeData(
            node_id=node_id,
            status=status,
            accel=(ax, ay, az),
            gyro=(gx, gy, gz),
            mag=(mx, my, mz),
        ))
        off += NODE_SIZE

    return Frame(
        version=version,
        hand=hand,
        seq=seq,
        timestamp_us=timestamp_us,
        odr_hz=odr_hz,
        nodes=nodes,
        flags=flags,
        node_count=node_count,
    )


def build_frame(frame: Frame) -> bytes:
    """将 Frame 编码为 338B 字节（CRC 自动计算）。主要供自测/回放使用。"""
    parts = bytearray()
    parts += _HEADER.pack(
        MAGIC,
        frame.version,
        frame.hand,
        frame.seq & 0xFFFFFFFF,
        frame.timestamp_us & 0xFFFFFFFF,
        frame.node_count,
        frame.odr_hz,
        frame.flags,
    )
    for n in frame.nodes:
        parts += _NODE.pack(
            n.node_id, n.status,
            n.accel[0], n.accel[1], n.accel[2],
            n.gyro[0], n.gyro[1], n.gyro[2],
            n.mag[0], n.mag[1], n.mag[2],
        )
    parts += _CRC.pack(crc16(bytes(parts)))
    return bytes(parts)
