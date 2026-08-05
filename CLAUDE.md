# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Fred is a fleet of small home robots (f0..fN), one directory per robot. Two active builds:

- **f1** (building now): a watcher — mostly stationary with the motor driver asleep, watching through an AMG8833 8x8 IR thermal camera; occasional voltage-scaled look-arounds, a double-check and tentative approach when a new heat source appears, pickup detection via gyro; an asymmetric tiredness-scaled sleep/wake rhythm, deep sleep from power-on until first woken. Deliberately **silent** — expression is light and motion only; voice arrives with f2's buzzer. Don't suggest adding sound to f1. Currently a set of primitives plus logging (motor layer, performances, recorder, calibration, pickup); the behaviour loop is designed in f1/doc/firmware.md, whose "Measured facts" section preserves lessons from the trialled-and-removed heat-seeking hunt. ESP32-S3-DevKitC-1 on the blue octagon chassis, two TT 1:90 motors and a caster through a single DRV8833, 4xAA NiMH pack. ESP32-S3-DevKitC-1 on the blue octagon chassis, two TT 1:90 motors and a caster through a single DRV8833, 4xAA NiMH pack.
- **f2** (next; parts not ordered - design can change): f1 evolved — Waveshare NS chassis, four TT motors (paired per side, skid steer) through two DRV8833s, an I2C suite (ToF, buzzer, LED matrix), 2S LiPo with a 5V buck, switched by an XT60 pull-loop key.

## Layout

- `README.md` — the fleet: members, common patterns, ESP-NOW comms protocol.
- `f0/` — chipless analog button-bot; docs only (`README.md`, `doc/parts.md`, `doc/schematic.svg`). No code, by rule.
- `f1/`, `f2/` — one directory per robot, same shape: `README.md`; `doc/schematic.md` (wiring and GPIO assignments), `doc/parts.md`, `doc/firmware.md` (behaviour design and measured facts), `doc/notes.md` (build lessons learned — consult before wiring the next robot); `firmware/` — an ESP-IDF project (target: esp32s3) with all application code in `firmware/main/firmware_main.c`, a single C file; `log/` — reference recorder dumps worth keeping, named descriptively (e.g. `quiet_room_spin.log`) — transient test dumps stay untracked in `firmware/` as `log.firmware.*`.

## Build and flash

Requires ESP-IDF (installed at `~/esp/esp-idf`). From `f1/firmware/` or `f2/firmware/`:

```sh
source ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # Ctrl+] to exit monitor
```

f1 splits this into `firmware/flash.sh` and `firmware/monitor.sh` — monitor attaches with `--no-reset` so a PSRAM recording survives (see f1/doc/firmware.md); f2's `firmware/firmware.sh` still captures the combined flash-and-monitor sequence. There are no tests or linters.

`sdkconfig.defaults` sets the important board options: 8 MB flash and octal PSRAM (non-fatal if absent).

## f1 GPIO map (from f1/doc/schematic.md)

| GPIO | Function |
|------|----------|
| 4, 5 | AIN1/AIN2 — right motor, inverted (PWM into DRV8833) |
| 6, 7 | BIN1/BIN2 — left motor, inverted (PWM into DRV8833) |
| 8, 9 | I2C SDA/SCL — QT chain: AMG8833 0x69, LSM6DSOX IMU 0x6A, INA219 power monitor 0x40 |
| 10 | DRV8833 nSLEEP — drive high to enable the motor driver |
| 38 | onboard WS2812 RGB status LED (red boot/fail, dim green asleep, green awake, blue performing/looking, amber whiff, violet held) |

## f2 GPIO map (from f2/doc/schematic.md)

| GPIO | Function |
|------|----------|
| 1 | battery voltage — ADC1_CH0, 100k/47k divider from the 7.4V rail |
| 4, 5 | left motor pair IN1/IN2 (PWM into DRV8833 #1, A+B inputs tied) |
| 6, 7 | right motor pair IN1/IN2 (PWM into DRV8833 #2, A+B inputs tied) |
| 8, 9 | I2C SDA/SCL — shared bus: AMG8833 0x69, VL53L4CD ToF 0x29, Qwiic Buzzer 0x34, bicolor 8x8 matrix 0x70 (STEMMA QT daisy-chain) |
| 10 | both DRV8833s' nSLEEP — drive high to enable the motor drivers |

Keep each robot's `doc/schematic.md` and the `#define`s in its `firmware_main.c` in sync if pins change.

## Notes

- The firmware uses the newer `driver/i2c_master.h` API (esp_driver_i2c component), not the legacy I2C driver.
- AMG8833 pixels are 12-bit two's complement, 0.25 °C per LSB, 64 pixels read from register 0x80.
- Naming: the fleet is f0..fN, each robot evolving from the last ("fleet", not "herd"). f0 is the chipless button-bot and must stay code-free.
- Parts lists do not include specific choices that could change from build to build if replicated like veroboard, wire, spacers, power switches and nuts/bolts etc.
