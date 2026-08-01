# Fred — wiring schematic

Single-board veroboard layout on the octagon chassis, board mounted
longitudinally: **ESP32-S3 at the front** (with the AMG8833 beside it),
**DRV8833 at the back** near the motors, maximising the distance between the
sensor/I2C end and the motor end. The switched 4xAA NiMH pack (~4.8-5.6 V)
sits on top and feeds both directly — the pack voltage suits the 3-6 V TT
motors as-is, and the DevKit's onboard LDO makes 3.3 V from it, so there is
no separate regulator.

## Power rails

Two veroboard strips at the **back** of the board serve as the 6V and GND
rails; everything taps power from them.

- **Tap ordering matters.** The pack leads enter the rails at the very back,
  with the DRV8833's VIN/GND taps immediately adjacent. The ESP32's 5V/GND
  taps sit further along the strips — never between the pack entry and the
  driver taps. Motor current is the big, spiky load; this ordering keeps it
  off any segment of strip the ESP32 shares, so it can't wobble the ESP32's
  supply or ground reference through the strip's resistance.
- **Beef up the rails.** A single veroboard strip is thin copper — marginal
  for two TT motors stalling. Flood the 6V and GND strips with solder or lay
  tinned copper wire along them, at minimum the segment between the pack
  entry and the driver taps (the only part carrying motor current).

## Capacitors

- **470-1000 µF electrolytic across the ESP32's 5V/GND taps.** Motor starts
  and stalls yank the pack voltage down through the cells' internal
  resistance; this cap rides through the dip and stops it resetting the
  chip. Mount it right where the ESP32's power wires leave the rails, and
  mind the polarity stripe.
- **100 µF electrolytic across the rails at the pack entry / driver taps.**
  A local reservoir for the motor current spikes, so they are served from
  millimetres away instead of travelling the strips. Polarity again.
- **2 off 100 nF ceramic, one across each motor's terminals.** Kills brush
  noise at the source; a different job from the electrolytics, not replaced
  by them.

## Wire list

| From | To | Wire | Notes |
|------|----|------|-------|
| Pack + / − | 6V / GND rails | red / black | enter at the back, next to the driver taps |
| 6V / GND rails | DRV8833 VIN / GND | red / black | immediately adjacent to pack entry |
| 6V / GND rails | ESP32 5V / GND pins | red / black | onboard LDO makes 3.3 V |
| ESP32 GPIO4 | DRV8833 AIN1 | any | left motor PWM |
| ESP32 GPIO5 | DRV8833 AIN2 | any | left motor PWM |
| ESP32 GPIO6 | DRV8833 BIN1 | any | right motor PWM |
| ESP32 GPIO7 | DRV8833 BIN2 | any | right motor PWM |
| ESP32 GPIO10 | DRV8833 SLP | any | drive high to enable; low = sleep |
| ESP32 3V3 | AMG8833 VIN | red | STEMMA QT colours shown |
| ESP32 GND | AMG8833 GND | black | to the ESP32 only — see Notes |
| ESP32 GPIO8 | AMG8833 SDA | blue | |
| ESP32 GPIO9 | AMG8833 SCL | yellow | |
| DRV8833 AOUT1/AOUT2 | left motor | red / black | swap to reverse direction |
| DRV8833 BOUT1/BOUT2 | right motor | red / black | swap to reverse direction |

## GPIO summary

| GPIO | Function |
|------|----------|
| 4 | left motor AIN1 (PWM) |
| 5 | left motor AIN2 (PWM) |
| 6 | right motor BIN1 (PWM) |
| 7 | right motor BIN2 (PWM) |
| 8 | I2C SDA (AMG8833) |
| 9 | I2C SCL (AMG8833) |
| 10 | DRV8833 sleep control |

## Notes

- **Common ground.** The shared GND strip is the return for the five control
  signals — with everything on one board no separate ground wire is needed
  between the ESP32 and the driver.
- **Sensor ground.** The AMG8833 grounds through its one black wire to the
  ESP32, nothing else. It draws ~5 mA and is entirely referenced to the
  ESP32; a second, direct run from the sensor to the GND rail would create a
  ground loop that motor switching noise couples into, riding straight on
  the I2C lines.
- **NiMH, not alkaline.** NiMH cells sag far less under motor load. If Fred
  ever resets when both motors stall, suspect tired or alkaline cells first.
- **Motor voltage.** TT motors are rated 3-6 V and the pack never exceeds
  that, so full PWM duty is fine.
- **DRV8833 SLP.** The carrier pulls nSLEEP up so the driver is enabled by
  default; wiring it to GPIO10 lets firmware put the driver to sleep. FLT
  (fault output) is left unconnected.
- **USB + battery.** The DevKitC-1's USB 5 V comes in through a diode, so
  having USB and the pack connected at the same time is fine — handy for
  flashing while installed.
- **Noise.** Twist the wires to each motor, and route them away from the
  AMG8833's I2C wiring at the front.
