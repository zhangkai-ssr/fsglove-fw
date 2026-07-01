"""UDP 接收端：循环 recvfrom -> parse_frame -> yield 有效 Frame。

内部统计收包数、CRC（含长度/magic）失败数、按 seq 估算丢帧数。
"""

from __future__ import annotations

import socket
from dataclasses import dataclass, field
from typing import Iterator, Optional

from .protocol import FRAME_SIZE, Frame, parse_frame


@dataclass
class RxStats:
    """接收统计。"""

    packets: int = 0          # 收到的 UDP 包数
    valid: int = 0            # 解析成功的帧数
    bad: int = 0              # 长度/magic/CRC 失败数
    dropped: int = 0          # 按 seq 估算的丢帧数
    last_seq: Optional[int] = field(default=None, repr=False)

    def note_seq(self, seq: int) -> None:
        """按 seq 连续性估算丢帧（u32 回绕用 32 位掩码处理）。"""
        if self.last_seq is not None:
            gap = (seq - self.last_seq) & 0xFFFFFFFF
            if 1 < gap < 0x80000000:        # 正常增 1；跳变视为丢包，忽略乱序/复位
                self.dropped += gap - 1
        self.last_seq = seq

    @property
    def loss_rate(self) -> float:
        total = self.valid + self.dropped
        return (self.dropped / total) if total else 0.0


class UdpReceiver:
    """绑定 UDP 端口并产出解析后的 Frame。"""

    def __init__(self, bind_ip: str = "0.0.0.0", port: int = 18888,
                 recv_timeout: Optional[float] = None) -> None:
        self.bind_ip = bind_ip
        self.port = port
        self.recv_timeout = recv_timeout
        self.stats = RxStats()
        self._sock: Optional[socket.socket] = None

    def __enter__(self) -> "UdpReceiver":
        self.open()
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def open(self) -> None:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # 增大接收缓冲，降低高 ODR 下丢包概率
        try:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
        except OSError:
            pass
        s.bind((self.bind_ip, self.port))
        if self.recv_timeout is not None:
            s.settimeout(self.recv_timeout)
        self._sock = s

    def close(self) -> None:
        if self._sock is not None:
            self._sock.close()
            self._sock = None

    def recv_frames(self) -> Iterator[Frame]:
        """阻塞式生成器：持续 yield 有效 Frame。无效包计入统计后跳过。"""
        for frame, _raw in self.recv_frames_raw():
            yield frame

    def recv_frames_raw(self) -> "Iterator[tuple]":
        """同 recv_frames，但额外产出原始 338B 字节，供 .bin 记录使用。"""
        if self._sock is None:
            self.open()
        assert self._sock is not None
        while True:
            try:
                data, _addr = self._sock.recvfrom(FRAME_SIZE + 64)
            except socket.timeout:
                continue
            self.stats.packets += 1
            frame = parse_frame(data)
            if frame is None:
                self.stats.bad += 1
                continue
            self.stats.valid += 1
            self.stats.note_seq(frame.seq)
            yield frame, data
