# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Fred is a fleet of small home robots (f0..fN), one directory per robot. The current focus is **f1** (Fred himself): he spins slowly until he senses heat (via an AMG8833 8x8 IR thermal camera), then moves towards it. f1 hardware is an ESP32-S3-DevKitC-1 on a Waveshare NS chassis driving four TT gear motors (paired per side, skid steer) through two DRV8833 dual motor drivers, powered by a 2S LiPo — drivers fed directly, ESP32 via a 5V buck, switched by an XT60 pull-loop key.

## Layout

- `README.md` — the fleet: members, common patterns, ESP-NOW comms protocol.
- `f0/` — chipless analog button-bot; docs only (`README.md`, `doc/parts.md`, `doc/schematic.svg`). No code, by rule.
- `f1/firmware/` — ESP-IDF project (target: esp32s3). All application code is in `f1/firmware/main/firmware_main.c`, a single C file.
- `f1/doc/schematic.md` — wiring and GPIO assignments; `f1/doc/parts.md` — parts list; `f1/doc/firmware.md` — heat-seeking algorithm design and open wrinkles.

## Build and flash

Requires ESP-IDF (installed at `~/esp/esp-idf`). From `f1/firmware/`:

```sh
source ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # Ctrl+] to exit monitor
```

`f1/firmware/firmware.sh` captures this flash-and-monitor sequence. There are no tests or linters.

`sdkconfig.defaults` sets the important board options: 8 MB flash and octal PSRAM (non-fatal if absent).

## f1 GPIO map (from f1/doc/schematic.md)

| GPIO | Function |
|------|----------|
| 1 | battery voltage — ADC1_CH0, 100k/47k divider from the 7.4V rail |
| 4, 5 | left motor pair IN1/IN2 (PWM into DRV8833 #1, A+B inputs tied) |
| 6, 7 | right motor pair IN1/IN2 (PWM into DRV8833 #2, A+B inputs tied) |
| 8, 9 | I2C SDA/SCL — shared bus: AMG8833 0x69, VL53L4CD ToF 0x29, Qwiic Buzzer 0x34, bicolor 8x8 matrix 0x70 (STEMMA QT daisy-chain) |
| 10 | both DRV8833s' nSLEEP — drive high to enable the motor drivers |

Keep `f1/doc/schematic.md` and the `#define`s in `firmware_main.c` in sync if pins change.

## Notes

- The firmware uses the newer `driver/i2c_master.h` API (esp_driver_i2c component), not the legacy I2C driver.
- AMG8833 pixels are 12-bit two's complement, 0.25 °C per LSB, 64 pixels read from register 0x80.
- Naming: the fleet is f0..fN, each robot evolving from the last ("fleet", not "herd"). f1 is Fred himself; f0 is the chipless button-bot and must stay code-free.

