# f1

f1 is a watcher: mostly stationary, eyes open (an AMG8833 8x8 thermal
camera), motor driver asleep. Now and again he looks around, and a new
source of warmth — someone arriving — earns a double-check and maybe a
tentative move their way. He knows when he's been picked up. The
essential Fred: thermal eyes, two motors, one driver, nothing else.

ESP32-S3-DevKitC-1 on the blue octagon chassis: two TT 1:90 gear motors
and a caster through a single DRV8833, powered by a 4xAA NiMH pack.

The build is proudly Heath Robinson: all the power lives in the octagon's
cavity (pack central for balance, motors, driver and rails at the back),
while the ESP32 veroboard rides above it on posts, on a little plastic
shelf, in full view. The AMG8833 pokes out the front on 6 mm standoffs.

## Docs

- [Parts](./doc/parts.md)
- [Schematic](./doc/schematic.md)
- [Firmware design](./doc/firmware.md)
- [Notes](./doc/notes.md)

## Build and flash

ESP-IDF project (target: esp32s3), all application code in
`firmware/main/firmware_main.c`. From `firmware/`:

```sh
source ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # Ctrl+] to exit monitor
```

`firmware/firmware.sh` captures the flash-and-monitor sequence.
