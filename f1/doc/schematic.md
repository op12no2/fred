# Fred — wiring schematic

Waveshare Robot Chassis NS: two mounting plates, four 6-9 V TT motors
(skid steer, one pair per side). The veroboard power/driver board sits on
the lower plate: **ESP32-S3 at the front** (with the AMG8833 beside it),
**two DRV8833s at the back** near the motors. The 2S LiPo (7.4 V nominal,
8.4 V full) feeds the motor drivers directly and the ESP32 through a
Pololu D24V22F5 5 V buck. Power is switched by an XT60 pull-loop key —
there is no switch.

## Power

- **Pull-loop key.** The pack's positive lead runs through *both contacts*
  of a chassis-mounted female XT60; the key is a male XT60 with its pins
  bridged by a heatshrunk wire loop. Key in = running; key out = air gap,
  truly zero draw. Female on the live side keeps the battery-live contact
  recessed. Tether the key to the chassis; mount it sticking up through
  the top plate like a wind-up key.
- **Rails.** 7.4 V and GND veroboard strips at the back. Pack entry (via
  the key) at the very back, both DRV8833 VIN/GND taps immediately
  adjacent, the buck's VIN/GND further along — motor current never flows
  through a strip segment the buck shares. Flood the rail strips with
  solder as before.
- **Buck.** D24V22F5 VOUT (5 V) → ESP32 5V pin (onboard LDO makes 3.3 V).
  Worst-case 5 V load (radio burst + matrix lit + buzzer + ToF) is ~1 A
  against its 2.5 A rating. The motors never touch the buck.
- **Battery monitor.** 100k from the 7.4 V rail to GPIO1, 47k from GPIO1
  to GND: 8.4 V max → 2.7 V at the pin (ADC1_CH0). GPIO1 deliberately:
  ADC1 works while the radio is active, ADC2 does not. Firmware must
  treat ~6.6 V (3.3 V/cell) as empty — stop, show empty battery on the
  matrix, sad chirp. Nothing else protects the LiPo from over-discharge.
  The divider's ~57 µA drain flows only when the key is in.

## Capacitors

- **100 µF electrolytic (16 V+) across each DRV8833 VIN/GND** at the
  carrier — the Pololu carriers have no bulk capacitance of their own.
  Mind the polarity stripes.
- **4 off 100 nF ceramic, one across each motor's terminals.** Kills
  brush noise at the source.
- The old 470-1000 µF brownout cap is gone: its job was riding out NiMH
  sag, and the 30C LiPo plus regulated buck removed the problem.

## Wire list

| From | To | Wire | Notes |
|------|----|------|-------|
| Pack XT60 | key socket contact A | red | male pigtail mates the pack |
| Key socket contact B | 7.4V rail | red | key bridges A→B; pulled = off |
| Pack − | GND rail | black | |
| 7.4V / GND rails | DRV8833 #1 VIN / GND | red / black | left-side driver, next to pack entry |
| 7.4V / GND rails | DRV8833 #2 VIN / GND | red / black | right-side driver, next to pack entry |
| 7.4V / GND rails | buck VIN / GND | red / black | further along the strips |
| Buck VOUT | ESP32 5V pin | red | return via GND rail |
| 7.4V rail | 100k → GPIO1 | any | divider top |
| GPIO1 | 47k → GND | any | divider bottom |
| ESP32 GPIO4 | DRV8833 #1 AIN1 + BIN1 (tied) | any | left pair PWM |
| ESP32 GPIO5 | DRV8833 #1 AIN2 + BIN2 (tied) | any | left pair PWM |
| ESP32 GPIO6 | DRV8833 #2 AIN1 + BIN1 (tied) | any | right pair PWM |
| ESP32 GPIO7 | DRV8833 #2 AIN2 + BIN2 (tied) | any | right pair PWM |
| ESP32 GPIO10 | both DRV8833 SLP (tied) | any | drive high to enable; low = sleep |
| ESP32 3V3 | AMG8833 VIN | red | STEMMA QT colours shown |
| ESP32 GND | AMG8833 GND | black | to the ESP32 only — see Notes |
| ESP32 GPIO8 | AMG8833 SDA | blue | |
| ESP32 GPIO9 | AMG8833 SCL | yellow | |
| AMG8833 spare STEMMA QT port | LED matrix backpack | 100mm QT cable | daisy-chain carries 3V3/GND/SDA/SCL |
| Matrix spare STEMMA QT port | Qwiic Buzzer | 100mm QT cable | |
| Buzzer spare STEMMA QT port | VL53L4CD ToF | 100mm QT cable | |
| DRV8833 #1 AOUT1/AOUT2 | front-left motor | red / black | swap to reverse direction |
| DRV8833 #1 BOUT1/BOUT2 | rear-left motor | red / black | match front-left's direction |
| DRV8833 #2 AOUT1/AOUT2 | front-right motor | red / black | swap to reverse direction |
| DRV8833 #2 BOUT1/BOUT2 | rear-right motor | red / black | match front-right's direction |

## GPIO summary

| GPIO | Function |
|------|----------|
| 1 | battery voltage (ADC1_CH0, 100k/47k divider from 7.4V rail) |
| 4 | left motor pair IN1 (PWM, DRV8833 #1 A+B channels) |
| 5 | left motor pair IN2 (PWM, DRV8833 #1 A+B channels) |
| 6 | right motor pair IN1 (PWM, DRV8833 #2 A+B channels) |
| 7 | right motor pair IN2 (PWM, DRV8833 #2 A+B channels) |
| 8 | I2C SDA (shared bus, see below) |
| 9 | I2C SCL (shared bus, see below) |
| 10 | both DRV8833s' sleep control |

## I2C bus

One bus on GPIO8/9; the boards daisy-chain over STEMMA QT/Qwiic cables
starting from the AMG8833's spare port (chain order is electrically
irrelevant — mount for cable convenience). All addresses are 7-bit defaults,
no clashes:

| Address | Device |
|---------|--------|
| 0x29 | VL53L4CD time-of-flight (front, angled down) |
| 0x34 | SparkFun Qwiic Buzzer |
| 0x69 | AMG8833 thermal camera |
| 0x70 | HT16K33 bicolor 8x8 LED matrix backpack |

Keep these in sync with the `#define`s in `firmware_main.c`.

## Notes

- **One H-bridge per motor.** Each side's two motors get their own DRV8833
  channel; only the *inputs* are paralleled, so the firmware still sees
  two motors' worth of GPIOs (4/5 left, 6/7 right) and each channel stays
  well inside its 1.2 A rating. Never parallel two motors on one channel.
- **Common ground.** The shared GND strip is the return for all control
  signals — everything is on one board.
- **Sensor ground.** The whole I2C chain grounds through the AMG8833's one
  black wire to the ESP32, nothing else — the daisy-chain cables carry GND
  from board to board. Everything is referenced to the ESP32; a second,
  direct run from any board to the GND rail would create a ground loop that
  motor switching noise couples into, riding straight on the I2C lines.
- **LiPo care.** Balance-charge only; pull the key for storage and never
  leave the pack flat. The firmware low-voltage cutoff (see Power) is the
  only over-discharge protection. Mount the pack where it can't be
  punctured — soft cells plus small children deserve a tray or enclosure.
- **Motor voltage.** The NS kit motors are rated 6-9 V; the 2S pack's full
  8.4 V is in range, so full PWM duty is fine.
- **Matrix brightness.** The whole I2C chain is fed from the ESP32's 3V3
  pin (onboard LDO). The camera, buzzer and ToF draw a few mA each, but
  the LED matrix can pull a couple hundred mA at full brightness — run it
  dimmed (HT16K33 has 16 global levels).
- **ToF mounting.** The VL53L4CD sits at the front, tilted down so its
  narrow 18° beam hits the floor ~15-20 cm ahead. One beam, three
  readings: steady = floor, shorter = obstacle, suddenly longer = a step
  down (the conservatory cliff).
- **USB + battery.** The DevKitC-1's USB 5 V comes in through a diode, so
  USB and the pack can be connected at the same time — handy for flashing
  while installed.
- **Noise.** Twist the wires to each motor, and route them away from the
  I2C wiring at the front.
