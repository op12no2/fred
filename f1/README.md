# f1 — Fred

Fred spins slowly until he senses heat, then moves towards it; that's the
core of him. Around that core: a face, a voice, a distance sense, and a
radio.

## Hardware

- **Chassis:** Waveshare NS, four 6-9V TT motors paired per side (skid
  steer), driven by two DRV8833s.
- **Brain:** ESP32-S3-DevKitC-1.
- **Senses & expression**, all on one I2C bus of STEMMA QT/Qwiic boards:
  AMG8833 8x8 thermal camera (the eyes), VL53L4CD time-of-flight (one
  downward-tilted beam: floor / obstacle / cliff), bicolor 8x8 LED matrix
  (the face — it can mirror the thermal view, green to red), Qwiic Buzzer
  (the voice).
- **Power:** 2S LiPo through an XT60 pull-loop key; motors fed directly,
  ESP32 via a 5V buck; battery voltage watched on an ADC divider.

## Docs

- [Parts](./doc/parts.md)
- [Schematic](./doc/schematic.md)
- [Firmware design](./doc/firmware.md)

## Build and flash

ESP-IDF project (target: esp32s3), all application code in
`firmware/main/firmware_main.c`. From `firmware/`:

```sh
source ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # Ctrl+] to exit monitor
```

`firmware/firmware.sh` captures the flash-and-monitor sequence.
