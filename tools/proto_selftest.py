"""协议一致性自测：构造一帧 -> 校验长度/CRC 自洽 -> parse_frame 往返一致。

直接运行: python tools/proto_selftest.py
退出码 0 表示全部通过。
"""

from __future__ import annotations

import os
import sys

# 让脚本能找到 host/ 下的 fsglove_host 包
_THIS = os.path.dirname(os.path.abspath(__file__))
_HOST = os.path.join(_THIS, "..", "host")
sys.path.insert(0, os.path.abspath(_HOST))

from fsglove_host.protocol import (  # noqa: E402
    FRAME_SIZE, NODE_COUNT, MAGIC, PROTO_VERSION,
    NodeData, Frame, crc16, parse_frame, build_frame,
)


def _make_frame() -> Frame:
    nodes = []
    for i in range(NODE_COUNT):
        nodes.append(NodeData(
            node_id=i,
            status=0x1F,                                  # 全部状态位置位
            accel=(100 + i, -200 - i, 300 + i),
            gyro=(-1 - i, 2 + i, -3 - i),
            mag=(i * 10, -i * 10, i * 5),
        ))
    return Frame(
        version=PROTO_VERSION, hand=1, seq=0xDEADBEEF,
        timestamp_us=0x01234567, odr_hz=200, nodes=nodes,
        flags=0xA5, node_count=NODE_COUNT,
    )


def main() -> int:
    failures = []

    def check(cond, msg):
        if not cond:
            failures.append(msg)
            print(f"  FAIL: {msg}")
        else:
            print(f"  ok:   {msg}")

    print("[proto_selftest] 开始")

    # 1) 帧长常量
    check(FRAME_SIZE == 338, f"FRAME_SIZE == 338 (实际 {FRAME_SIZE})")

    # 2) CRC 已知向量（CRC-16/CCITT-FALSE: '123456789' -> 0x29B1）
    check(crc16(b"123456789") == 0x29B1,
          f"crc16('123456789') == 0x29B1 (实际 0x{crc16(b'123456789'):04X})")

    # 3) 构造帧字节长度
    frame = _make_frame()
    buf = build_frame(frame)
    check(len(buf) == FRAME_SIZE, f"build_frame 长度 == {FRAME_SIZE} (实际 {len(buf)})")

    # 4) magic 小端正确
    check(buf[0] == 0x53 and buf[1] == 0x47, "magic 字节 = 0x53 0x47 (小端 0x4753)")
    check(MAGIC == 0x4753, "MAGIC 常量 == 0x4753")

    # 5) CRC 自洽：末 2B == crc16(前 336B)
    crc_in_buf = buf[336] | (buf[337] << 8)
    check(crc_in_buf == crc16(buf[:336]), "帧内 CRC == crc16(前336B)")

    # 6) parse_frame 往返一致
    parsed = parse_frame(buf)
    check(parsed is not None, "parse_frame 返回非 None")
    if parsed is not None:
        check(parsed.seq == frame.seq, "seq 往返一致")
        check(parsed.timestamp_us == frame.timestamp_us, "timestamp_us 往返一致")
        check(parsed.odr_hz == frame.odr_hz, "odr_hz 往返一致")
        check(parsed.hand == frame.hand, "hand 往返一致")
        check(parsed.flags == frame.flags, "flags 往返一致")
        check(len(parsed.nodes) == NODE_COUNT, "节点数 == 16")
        same = all(
            p.node_id == o.node_id and p.status == o.status
            and p.accel == o.accel and p.gyro == o.gyro and p.mag == o.mag
            for p, o in zip(parsed.nodes, frame.nodes)
        )
        check(same, "全部节点字段往返一致")

    # 7) 异常路径：坏长度 / 坏 magic / 坏 CRC 均返回 None
    check(parse_frame(buf[:-1]) is None, "短一字节 -> None")
    bad_magic = bytearray(buf); bad_magic[0] ^= 0xFF
    check(parse_frame(bytes(bad_magic)) is None, "坏 magic -> None")
    bad_crc = bytearray(buf); bad_crc[336] ^= 0xFF
    check(parse_frame(bytes(bad_crc)) is None, "坏 CRC -> None")

    if failures:
        print(f"[proto_selftest] 失败 {len(failures)} 项")
        return 1
    print("[proto_selftest] 全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
