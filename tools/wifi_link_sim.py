"""Small ESP32 Wi-Fi relay simulation.

It checks the intended firmware behavior:
STM32 frames keep arriving, ESP32 sends them over UDP when Wi-Fi is connected,
and reconnect attempts bound the disconnected state.
"""

from __future__ import annotations

import argparse
import random
from dataclasses import dataclass

FRAME_SIZE = 338


@dataclass
class Result:
    name: str
    frames: int
    sent: int
    failed: int
    retry_count: int
    stopped: bool


def in_window(seq: int, windows: tuple[tuple[int, int], ...]) -> bool:
    return any(start <= seq < end for start, end in windows)


def simulate(
    name: str,
    frames: int,
    outage_windows: tuple[tuple[int, int], ...] = (),
    udp_loss: float = 0.0,
    max_retry: int = 0,
    retry_every_frames: int = 10,
    seed: int = 1,
) -> Result:
    rng = random.Random(seed)
    connected = True
    stopped = False
    retry_count = 0
    sent = 0
    failed = 0

    for seq in range(frames):
        radio_down = in_window(seq, outage_windows)
        if radio_down:
            connected = False
            if seq % retry_every_frames == 0 and not stopped:
                if max_retry and retry_count >= max_retry:
                    stopped = True
                else:
                    retry_count += 1
        elif not stopped:
            connected = True
            retry_count = 0

        if connected and rng.random() >= udp_loss:
            sent += 1
        else:
            failed += 1

    return Result(name, frames, sent, failed, retry_count, stopped)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=10)
    ap.add_argument("--frames", type=int, default=400)
    args = ap.parse_args()

    stable = simulate("stable_wifi", args.frames)
    transient = simulate("transient_reconnect", args.frames, ((80, 120), (250, 270)))
    exhausted = simulate("retry_exhausted", args.frames, ((20, args.frames),), max_retry=3)

    assert stable.sent == args.frames and stable.failed == 0
    assert transient.sent == args.frames - 60 and transient.failed == 60
    assert exhausted.stopped and exhausted.sent == 20

    print("case,frames,sent,failed,retry_count,stopped,throughput_kbps")
    for r in (stable, transient, exhausted):
        kbps = r.sent * FRAME_SIZE * 8 / 2.0 / 1000.0
        print(f"{r.name},{r.frames},{r.sent},{r.failed},{r.retry_count},{int(r.stopped)},{kbps:.1f}")

    for seed in range(args.runs):
        loss = 0.01 + (seed % 5) * 0.005
        r = simulate(f"random_loss_{seed}", args.frames, udp_loss=loss, seed=seed)
        assert r.sent + r.failed == args.frames
        assert r.sent >= int(args.frames * 0.95)
        kbps = r.sent * FRAME_SIZE * 8 / 2.0 / 1000.0
        print(f"{r.name},{r.frames},{r.sent},{r.failed},{r.retry_count},{int(r.stopped)},{kbps:.1f}")


if __name__ == "__main__":
    main()
