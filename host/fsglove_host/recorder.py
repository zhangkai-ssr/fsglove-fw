"""帧记录器：CSV（每节点每帧一行）与可选原始二进制 .bin（直接落 338B 帧）。

上下文管理器风格：
    with Recorder(csv_path="run.csv") as rec:
        rec.write(frame, raw=raw_bytes)
"""

from __future__ import annotations

import csv
from typing import Optional

from .protocol import Frame

# CSV 列：seq, timestamp_us, node_id, 9 轴, status
CSV_HEADER = [
    "seq", "timestamp_us", "node_id",
    "ax", "ay", "az",
    "gx", "gy", "gz",
    "mx", "my", "mz",
    "status",
]


class Recorder:
    """可同时写 CSV 与原始 .bin。任一路径为 None 即跳过该格式。"""

    def __init__(self, csv_path: Optional[str] = None,
                 bin_path: Optional[str] = None) -> None:
        self.csv_path = csv_path
        self.bin_path = bin_path
        self._csv_file = None
        self._csv_writer = None
        self._bin_file = None
        self.rows = 0
        self.frames = 0

    def __enter__(self) -> "Recorder":
        self.open()
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def open(self) -> None:
        if self.csv_path:
            self._csv_file = open(self.csv_path, "w", newline="", encoding="utf-8")
            self._csv_writer = csv.writer(self._csv_file)
            self._csv_writer.writerow(CSV_HEADER)
        if self.bin_path:
            self._bin_file = open(self.bin_path, "wb")

    def close(self) -> None:
        if self._csv_file is not None:
            self._csv_file.close()
            self._csv_file = None
            self._csv_writer = None
        if self._bin_file is not None:
            self._bin_file.close()
            self._bin_file = None

    def write(self, frame: Frame, raw: Optional[bytes] = None) -> None:
        """写一帧。raw 为原始 338B 字节（写 .bin 用），缺省则不写二进制。"""
        if self._csv_writer is not None:
            for n in frame.nodes:
                self._csv_writer.writerow([
                    frame.seq, frame.timestamp_us, n.node_id,
                    n.accel[0], n.accel[1], n.accel[2],
                    n.gyro[0], n.gyro[1], n.gyro[2],
                    n.mag[0], n.mag[1], n.mag[2],
                    n.status,
                ])
                self.rows += 1
        if self._bin_file is not None and raw is not None:
            self._bin_file.write(raw)
        self.frames += 1
