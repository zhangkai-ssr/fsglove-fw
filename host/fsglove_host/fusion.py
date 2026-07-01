"""姿态融合占位实现：每节点一个 Mahony AHRS 滤波器。

这是最小可用 Mahony 实现，用于打通"原始数据 -> 四元数"链路；
精度/标定/坐标系对齐需后续按实际传感器量程与安装姿态替换。

注意：原始 accel/gyro/mag 为 i16 计数值，未做单位换算（应除以各自灵敏度）。
本占位对 accel/mag 仅做归一化，故量程不影响方向；gyro 需为 rad/s，
此处提供 gyro_scale 形参由调用方传入（默认 1.0 即视输入已为 rad/s）。
"""

from __future__ import annotations

import math
from typing import Dict, Optional

import numpy as np

from .protocol import NODE_COUNT, Frame, ST_MAG_OK


class MahonyAHRS:
    """单节点 Mahony 互补滤波 AHRS。"""

    def __init__(self, kp: float = 1.0, ki: float = 0.0,
                 gyro_scale: float = 1.0) -> None:
        self.kp = kp
        self.ki = ki
        self.gyro_scale = gyro_scale          # 计数值 -> rad/s 的换算系数
        self.q = np.array([1.0, 0.0, 0.0, 0.0])   # (w, x, y, z)
        self._integral = np.zeros(3)              # 积分误差项

    def reset(self) -> None:
        self.q = np.array([1.0, 0.0, 0.0, 0.0])
        self._integral[:] = 0.0

    def update(self, accel, gyro, mag, dt: float):
        """推进一步。accel/gyro/mag 为三元组；mag 可为 None 退化为 6 轴。"""
        gx, gy, gz = (np.asarray(gyro, dtype=float) * self.gyro_scale)

        a = np.asarray(accel, dtype=float)
        use_mag = mag is not None and np.any(np.asarray(mag, dtype=float) != 0.0)

        # 加速度计无效时直接积分陀螺
        if np.linalg.norm(a) > 1e-9:
            a = a / np.linalg.norm(a)
            q0, q1, q2, q3 = self.q

            # 由当前姿态估计的重力方向
            v = np.array([
                2.0 * (q1 * q3 - q0 * q2),
                2.0 * (q0 * q1 + q2 * q3),
                q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3,
            ])

            if use_mag:
                m = np.asarray(mag, dtype=float)
                m = m / np.linalg.norm(m)
                # 参考磁场（地球坐标系 -> 机体），9 轴更新
                h = self._rotate_body_to_earth(m)
                bx = math.hypot(h[0], h[1])
                bz = h[2]
                w = np.array([
                    2.0 * bx * (0.5 - q2 * q2 - q3 * q3) + 2.0 * bz * (q1 * q3 - q0 * q2),
                    2.0 * bx * (q1 * q2 - q0 * q3) + 2.0 * bz * (q0 * q1 + q2 * q3),
                    2.0 * bx * (q0 * q2 + q1 * q3) + 2.0 * bz * (0.5 - q1 * q1 - q2 * q2),
                ])
                e = np.cross(a, v) + np.cross(m, w)
            else:
                e = np.cross(a, v)          # 6 轴 IMU 更新

            if self.ki > 0.0:
                self._integral += e * dt
            else:
                self._integral[:] = 0.0

            gx += self.kp * e[0] + self.ki * self._integral[0]
            gy += self.kp * e[1] + self.ki * self._integral[1]
            gz += self.kp * e[2] + self.ki * self._integral[2]

        # 四元数微分积分
        q0, q1, q2, q3 = self.q
        qdot = 0.5 * np.array([
            -q1 * gx - q2 * gy - q3 * gz,
            q0 * gx + q2 * gz - q3 * gy,
            q0 * gy - q1 * gz + q3 * gx,
            q0 * gz + q1 * gy - q2 * gx,
        ])
        self.q = self.q + qdot * dt
        n = np.linalg.norm(self.q)
        if n > 1e-12:
            self.q = self.q / n
        return tuple(self.q)

    def _rotate_body_to_earth(self, v: np.ndarray) -> np.ndarray:
        """用当前姿态把机体向量旋到地球坐标系。"""
        q0, q1, q2, q3 = self.q
        R = np.array([
            [q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3,
             2 * (q1 * q2 - q0 * q3),
             2 * (q1 * q3 + q0 * q2)],
            [2 * (q1 * q2 + q0 * q3),
             q0 * q0 - q1 * q1 + q2 * q2 - q3 * q3,
             2 * (q2 * q3 - q0 * q1)],
            [2 * (q1 * q3 - q0 * q2),
             2 * (q2 * q3 + q0 * q1),
             q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3],
        ])
        return R @ v


class FusionBank:
    """管理 NODE_COUNT 个 Mahony 滤波器，按帧批量处理。"""

    def __init__(self, kp: float = 1.0, ki: float = 0.0,
                 gyro_scale: float = 1.0) -> None:
        self.filters = [
            MahonyAHRS(kp=kp, ki=ki, gyro_scale=gyro_scale)
            for _ in range(NODE_COUNT)
        ]
        self._last_ts_us: Optional[int] = None

    def process(self, frame: Frame, dt: Optional[float] = None) -> Dict[int, tuple]:
        """处理一帧，返回 {node_id: (w,x,y,z)}。

        dt 缺省时由相邻帧 timestamp_us 估算（u32 微秒，含回绕处理）。
        """
        if dt is None:
            if self._last_ts_us is None:
                dt = 1.0 / max(frame.odr_hz, 1)
            else:
                delta = (frame.timestamp_us - self._last_ts_us) & 0xFFFFFFFF
                dt = delta / 1e6 if 0 < delta < 0x80000000 else 1.0 / max(frame.odr_hz, 1)
            self._last_ts_us = frame.timestamp_us

        result: Dict[int, tuple] = {}
        for node in frame.nodes:
            idx = node.node_id if node.node_id < NODE_COUNT else None
            if idx is None:
                continue
            mag = node.mag if (node.status & ST_MAG_OK) else None
            q = self.filters[idx].update(node.accel, node.gyro, mag, dt)
            result[node.node_id] = q
        return result
