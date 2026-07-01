"""FSGlove v2 上位机命令行入口。

用法:
    python -m fsglove_host.main --fuse --record csv --out run.csv
"""

from __future__ import annotations

import argparse
import os
import sys
import time

from .protocol import ST_NODE_ONLINE
from .receiver import UdpReceiver
from .recorder import Recorder
from .fusion import FusionBank

# 录制输出目录：fsglove-fw/host/recordings（相对本包，位于 fsglove_host 的上一级）
RECORDINGS_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "recordings"
)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="fsglove_host",
        description="FSGlove v2 上位机：UDP 接收 -> 解析 -> (融合) -> 记录",
    )
    p.add_argument("--bind", default="0.0.0.0", help="绑定 IP（默认 0.0.0.0）")
    p.add_argument("--port", type=int, default=18888, help="UDP 端口（默认 18888）")
    p.add_argument("--record", choices=["csv", "bin", "none"], default="none",
                   help="记录格式（默认 none）")
    p.add_argument("--out", default=None, help="记录输出路径")
    p.add_argument("--fuse", action="store_true", help="启用 Mahony 姿态融合")
    p.add_argument("--print", dest="print_hz", type=float, default=1.0,
                   help="状态打印频率 Hz（默认 1.0，0 关闭）")
    return p


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)

    # 记录器配置：默认输出到 recordings/ 目录（--out 可覆盖）
    csv_path = bin_path = None
    if args.record == "csv":
        csv_path = args.out or os.path.join(RECORDINGS_DIR, "fsglove_run.csv")
    elif args.record == "bin":
        bin_path = args.out or os.path.join(RECORDINGS_DIR, "fsglove_run.bin")

    # 确保输出目录存在
    for _p in (csv_path, bin_path):
        if _p:
            os.makedirs(os.path.dirname(os.path.abspath(_p)), exist_ok=True)

    fusion = FusionBank() if args.fuse else None
    print_period = (1.0 / args.print_hz) if args.print_hz > 0 else None

    receiver = UdpReceiver(bind_ip=args.bind, port=args.port, recv_timeout=1.0)
    recorder = Recorder(csv_path=csv_path, bin_path=bin_path)

    print(f"[fsglove] 监听 {args.bind}:{args.port}  "
          f"record={args.record}  fuse={args.fuse}", flush=True)

    last_print = time.monotonic()
    last_valid = 0
    try:
        with receiver, recorder:
            for frame, raw in receiver.recv_frames_raw():
                if fusion is not None:
                    fusion.process(frame)        # 占位融合，结果暂不下游使用
                if args.record != "none":
                    recorder.write(frame, raw=raw)

                if print_period is not None:
                    now = time.monotonic()
                    if now - last_print >= print_period:
                        st = receiver.stats
                        elapsed = now - last_print
                        fps = (st.valid - last_valid) / elapsed if elapsed > 0 else 0.0
                        online = sum(1 for n in frame.nodes
                                     if n.status & ST_NODE_ONLINE)
                        print(f"[fsglove] fps={fps:6.1f}  valid={st.valid}  "
                              f"bad={st.bad}  dropped={st.dropped}  "
                              f"loss={st.loss_rate*100:4.1f}%  online={online}/{len(frame.nodes)}",
                              flush=True)
                        last_print = now
                        last_valid = st.valid
    except KeyboardInterrupt:
        print("\n[fsglove] 停止。", flush=True)

    st = receiver.stats
    print(f"[fsglove] 汇总: packets={st.packets} valid={st.valid} "
          f"bad={st.bad} dropped={st.dropped}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
