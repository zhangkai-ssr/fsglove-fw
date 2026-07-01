# FSGlove Firmware Agent Context

This is the active FSGlove v2 firmware/host code root.

## Project Map

- `firmware/stm32h7/` - STM32H750/H7 acquisition firmware.
- `firmware/esp32/` - ESP32 Wi-Fi relay firmware.
- `firmware/common/fsglove_protocol.h` - shared wire protocol, the source of truth.
- `host/` - Python UDP receiver, parser, optional fusion, recorder.
- `tools/` - small validation scripts, including protocol self-test.
- `../fsglove-main/` - upstream/original Go server plus Python/PyTorch/MANO visualizer/reconstruction stack. Do not assume it is the current MCU firmware.
- `../DB-V01/`, `../硬件打板/`, `../硬件打板资料/`, `../左手*/` - PCB/BOM/Gerber/STEP/export packages. Read these for hardware evidence; do not rewrite generated manufacturing outputs unless asked.

## Firmware Architecture

The active v2 data path is:

```text
16 IMU nodes -> STM32H7 acquisition/packing -> UART 3 Mbps -> ESP32 -> Wi-Fi UDP -> Python host
```

Key facts:

- STM32 target is `STM32H750IBK6` / `STM32H750xx`, 480 MHz, 128 KB internal flash.
- STM32 reads 16 nodes over 2 SPI buses, each node carrying accel/gyro plus optional mag.
- ESP32 does not sample sensors. It receives fixed frames from STM32, validates framing/CRC, then forwards by UDP.
- Frame format is fixed at 338 bytes: 16-byte header, 16 x 20-byte node payloads, 2-byte CRC16.
- CRC is CRC-16/CCITT-FALSE. Multi-byte fields are little-endian.

## Non-Negotiable Rules

- Do not duplicate or invent protocol structs. Change `firmware/common/fsglove_protocol.h` first, then update STM32, ESP32, and Python host together.
- Do not guess PCB-dependent mappings. `fsglove_config.h`, `cs_decoder.c`, GPIOs, HSE/PLLM, and connector mappings must be checked against the actual schematic/PCB/export files.
- Do not casually edit generated build trees, `_deps`, binary artifacts, Gerbers, STEP files, or zipped manufacturing packages.
- Keep firmware changes small and hardware-aware. Physical parameters such as ODR, SPI clock, UART baud, IMU calibration, and pin mapping need explicit values and verification.
- Preserve the simple current transport unless asked otherwise: STM32 -> UART -> ESP32 -> UDP.
- If a firmware behavior is temporarily disabled, prefer one reversible compile-time switch or narrow guard over deleting the behavior.

## Common Commands

Run from this directory unless noted.

```powershell
# STM32H7 build
cd firmware/stm32h7
cmake --preset stm32h7
cmake --build --preset stm32h7
```

```powershell
# ESP32 build
cd firmware/esp32
idf.py set-target esp32
idf.py build
```

```powershell
# Python host setup/run
cd host
pip install -r requirements.txt
python -m fsglove_host.main
```

```powershell
# Protocol self-test
python tools/proto_selftest.py
```

## Validation Expectations

- Protocol changes: run `tools/proto_selftest.py` and verify STM32/ESP32/Python sizes and CRC behavior match.
- STM32 changes: build `firmware/stm32h7`; check flash still fits H750 128 KB.
- ESP32 changes: build with ESP-IDF and confirm `main/fsg_config.h` still matches the STM32 UART settings.
- Host parser changes: run the protocol self-test and a minimal receive/parse path if touching `host/fsglove_host/protocol.py` or `receiver.py`.

## Known Open Hardware TODOs

- `firmware/stm32h7/App/fsglove_config.h`: `fsg_node_map[]` is still a PCB-specific mapping point.
- `firmware/stm32h7/App/cs_decoder.c`: 74HC154 GPIO mapping must match the real board.
- `firmware/stm32h7/Core/Src/main.c`: HSE mode and PLL settings may need board-specific changes.
- `firmware/esp32/main/fsg_config.h`: Wi-Fi SSID/password and host IP are placeholders until deployment.

