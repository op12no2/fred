# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Fred is a small robot: he spins slowly until he senses heat (via an AMG8833 8x8 IR thermal camera), then moves towards it. Hardware is an ESP32-S3-DevKitC-1 driving two TT gear motors through a DRV8833 dual motor driver, powered by a 4xAA NiMH pack.

## Layout

- `firmware/` — ESP-IDF project (target: esp32s3). All application code is in `firmware/main/firmware_main.c`, a single C file.
- `doc/schematic.md` — wiring and GPIO assignments; `doc/parts.md` — parts list.

## Build and flash

Requires ESP-IDF (installed at `~/esp/esp-idf`). From `firmware/`:

```sh
source ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # Ctrl+] to exit monitor
```

`firmware/firmware.sh` captures this flash-and-monitor sequence. There are no tests or linters.

`sdkconfig.defaults` sets the important board options: 8 MB flash and octal PSRAM (non-fatal if absent).

## GPIO map (from doc/schematic.md)

| GPIO | Function |
|------|----------|
| 4, 5 | left motor AIN1/AIN2 (PWM into DRV8833) |
| 6, 7 | right motor BIN1/BIN2 (PWM into DRV8833) |
| 8, 9 | I2C SDA/SCL to AMG8833 (addr 0x69) |
| 10 | DRV8833 nSLEEP — drive high to enable the motor driver |

Keep `doc/schematic.md` and the `#define`s in `firmware_main.c` in sync if pins change.

## Notes

- The firmware uses the newer `driver/i2c_master.h` API (esp_driver_i2c component), not the legacy I2C driver.
- AMG8833 pixels are 12-bit two's complement, 0.25 °C per LSB, 64 pixels read from register 0x80.
- Fred is the first in N robots, each evolving from the last.

