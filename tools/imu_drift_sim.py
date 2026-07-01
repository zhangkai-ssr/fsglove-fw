"""Stationary IMU drift simulation for the current board IMU path.

It models the firmware output after the netlist-driven change:
LSM6DSOX accel+gyro, with optional LIS3MDL yaw correction.
"""

from __future__ import annotations

import argparse
import math
import random

ACCEL_LSB_PER_G = 8192.0       # LSM6DSOX +-4g, close enough for drift testing.
GYRO_LSB_PER_DPS = 1000.0 / 70.0  # LSM6DSOX +-2000 dps, 70 mdps/LSB.


def quantize(value: float, scale: float) -> float:
    return round(value * scale) / scale


def wrap_deg(angle: float) -> float:
    return (angle + 180.0) % 360.0 - 180.0


def run(duration_s: float, odr_hz: float, calibrate_s: float, seed: int,
        use_mag: bool = False) -> dict[str, float]:
    rng = random.Random(seed)
    dt = 1.0 / odr_hz
    n = int(duration_s * odr_hz)
    cal_n = max(1, int(calibrate_s * odr_hz))

    # Deliberately modest bias: enough to show the behavior without pretending
    # this is a datasheet-accurate temperature chamber.
    gyro_bias0 = (0.12, -0.08, 0.30)             # dps
    gyro_bias_rate = (0.00005, -0.00003, 0.00005)  # dps/s after warm-up drift
    gyro_noise = 0.02                            # dps RMS
    accel_noise = 0.002                          # g RMS
    mag_yaw_bias = 1.5                           # deg residual hard/soft-iron error
    mag_yaw_noise = 0.8                          # deg RMS

    samples: list[tuple[tuple[float, float, float], tuple[float, float, float]]] = []
    for i in range(n):
        t = i * dt
        gyro = tuple(
            quantize(gyro_bias0[a] + gyro_bias_rate[a] * t + rng.gauss(0.0, gyro_noise),
                     GYRO_LSB_PER_DPS)
            for a in range(3)
        )
        accel = (
            quantize(rng.gauss(0.0, accel_noise), ACCEL_LSB_PER_G),
            quantize(rng.gauss(0.0, accel_noise), ACCEL_LSB_PER_G),
            quantize(1.0 + rng.gauss(0.0, accel_noise), ACCEL_LSB_PER_G),
        )
        samples.append((accel, gyro))

    offset = [0.0, 0.0, 0.0]
    if calibrate_s > 0:
        for _, gyro in samples[:cal_n]:
            for a in range(3):
                offset[a] += gyro[a]
        offset = [x / cal_n for x in offset]

    roll = pitch = yaw = 0.0
    max_roll = max_pitch = max_yaw = 0.0
    alpha = 0.98
    yaw_alpha = 0.995

    for accel, gyro_raw in samples:
        gx, gy, gz = (gyro_raw[a] - offset[a] for a in range(3))

        roll_acc = math.degrees(math.atan2(accel[1], accel[2]))
        pitch_acc = math.degrees(math.atan2(-accel[0], math.hypot(accel[1], accel[2])))

        roll = alpha * (roll + gx * dt) + (1.0 - alpha) * roll_acc
        pitch = alpha * (pitch + gy * dt) + (1.0 - alpha) * pitch_acc
        yaw += gz * dt
        if use_mag:
            mag_yaw = mag_yaw_bias + rng.gauss(0.0, mag_yaw_noise)
            yaw += (1.0 - yaw_alpha) * wrap_deg(mag_yaw - yaw)

        max_roll = max(max_roll, abs(roll))
        max_pitch = max(max_pitch, abs(pitch))
        max_yaw = max(max_yaw, abs(yaw))

    return {
        "final_roll_deg": roll,
        "final_pitch_deg": pitch,
        "final_yaw_deg": yaw,
        "max_roll_deg": max_roll,
        "max_pitch_deg": max_pitch,
        "max_yaw_deg": max_yaw,
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--duration", type=float, default=120.0)
    ap.add_argument("--odr", type=float, default=100.0)
    ap.add_argument("--calibrate", type=float, default=5.0)
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()

    raw = run(args.duration, args.odr, 0.0, args.seed)
    calibrated = run(args.duration, args.odr, args.calibrate, args.seed)
    mag = run(args.duration, args.odr, args.calibrate, args.seed, use_mag=True)

    assert abs(calibrated["final_yaw_deg"]) < abs(raw["final_yaw_deg"])
    assert mag["max_yaw_deg"] < 3.0
    assert calibrated["max_roll_deg"] < 1.0 and calibrated["max_pitch_deg"] < 1.0

    print(f"duration={args.duration:.0f}s odr={args.odr:.0f}Hz")
    print("case,final_roll_deg,final_pitch_deg,final_yaw_deg,max_roll_deg,max_pitch_deg,max_yaw_deg")
    for name, r in (
        ("uncalibrated_6axis", raw),
        ("startup_calibrated_6axis", calibrated),
        ("startup_calibrated_9axis_mag", mag),
    ):
        print(
            f"{name},"
            f"{r['final_roll_deg']:.3f},{r['final_pitch_deg']:.3f},{r['final_yaw_deg']:.3f},"
            f"{r['max_roll_deg']:.3f},{r['max_pitch_deg']:.3f},{r['max_yaw_deg']:.3f}"
        )


if __name__ == "__main__":
    main()
