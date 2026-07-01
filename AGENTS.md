# Agent Instructions

This file is for any coding agent working in this firmware/host code root. Keep changes grounded in the local checkout and avoid broad rewrites.

## First Read

Before editing, identify which part is relevant:

- STM32H7 acquisition firmware: `firmware/stm32h7/`
- ESP32 relay firmware: `firmware/esp32/`
- Shared protocol: `firmware/common/fsglove_protocol.h`
- Python receiver/recorder: `host/`
- Hardware evidence outside this code root: `../DB-V01/`, `../硬件打板/`, `../硬件打板资料/`, `../左手*/`

Do not treat parent workspace folders as this codebase.

## Active Firmware Scope

The intended runtime path is:

```text
STM32H7 samples and packs frames -> ESP32 validates and relays -> host receives UDP frames
```

## Editing Rules

- Prefer the smallest correct change in the owning module.
- Reuse existing module boundaries: `acquisition`, `imu_node`, `spi_bus`, `cs_decoder`, `esp_link`, `stm_link`, `wifi_sta`, `net_sender`, `protocol`.
- Protocol edits must be synchronized across:
  - `firmware/common/fsglove_protocol.h`
  - STM32 packing/checking code
  - ESP32 receiving/checking code
  - Python host parser/builder
- Do not edit generated or downloaded content unless explicitly asked:
  - `build/`
  - `_deps/`
  - Gerber/STEP/PickAndPlace/BOM export packages
  - zip/7z archives
- Do not guess hardware wiring. Use schematic/PCB/BOM evidence or leave a clear TODO.
- Keep Wi-Fi credentials, host IP, pins, ODR, UART baud, and calibration values explicit and easy to audit.

## Build And Check Commands

From this directory:

```powershell
python tools/proto_selftest.py
```

STM32H7:

```powershell
cd firmware/stm32h7
cmake --preset stm32h7
cmake --build --preset stm32h7
```

ESP32:

```powershell
cd firmware/esp32
idf.py set-target esp32
idf.py build
```

Python host:

```powershell
cd host
pip install -r requirements.txt
python -m fsglove_host.main
```

## Review Checklist

For firmware changes, check:

- Does STM32 and ESP32 agree on `FSG_FRAME_SIZE`, UART baud, and CRC?
- Did any protocol field change without updating Python host parsing?
- Does STM32 data sent by DMA live in DMA-accessible memory?
- Are H750 flash limits still respected?
- Are PCB-specific pin/CS changes backed by hardware files?
- Does the change preserve the low-latency UDP relay unless a reliable transport was requested?

## Current Known Gaps

- STM32 node-to-bus/CS mapping needs real PCB verification.
- 74HC154 GPIO mapping needs real PCB verification.
- STM32 sampling is currently SysTick-paced; TIM pacing is the likely upgrade when jitter matters.
- SPI reads are currently sequential/blocking; DMA parallel read is an upgrade only if measured throughput requires it.
- ESP32 Wi-Fi config defaults are placeholders.

