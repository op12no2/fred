# f1 — wiring schematic

Two-level layout on the octagon chassis. The **cavity holds everything
power**: the switched 4xAA NiMH pack (~4.8-5.6 V, central and slightly aft
for balance), the motors, and at the back a small power veroboard carrying
the DRV8833 and the supply rails. The **ESP32-S3 veroboard rides above the
cavity** on posts, mounted to a small plastic shelf, in full view. The
**AMG8833 hangs off the front outside** on 6 mm standoffs (extendable if its
view needs clearing). The pack feeds everything directly — the pack voltage
suits the 3-6 V TT motors as-is, and the DevKit's onboard LDO makes 3.3 V
from it, so there is no separate regulator.

The split is the layout's best electrical feature: motor current never
leaves the cavity, and the sensor/I2C end lives a level above it.

## Power rails

Two strips on the **power veroboard in the cavity** serve as the 6V and GND
rails; everything taps power from them.

- **Tap ordering matters.** The pack leads enter the rails at one end, with
  the DRV8833's VIN/GND taps immediately adjacent. The ESP32's feed taps sit
  further along the strips — never between the pack entry and the driver
  taps. Motor current is the big, spiky load; this ordering keeps it off any
  segment of strip the ESP32 shares, so it can't wobble the ESP32's supply
  or ground reference through the strip's resistance.
- **The ESP32 feed is a twisted pair up a post.** From its rail taps, run
  5V/GND as one twisted red/black pair up to the shelf board. Twisting keeps
  the loop area small so the pair neither radiates nor picks much up, and
  the run doubles as the ESP32's only ground reference — one pair, no second
  ground path.
- **Beef up the rails.** A single veroboard strip is thin copper — marginal
  for two TT motors stalling. Flood the 6V and GND strips with solder or lay
  tinned copper wire along them, at minimum the segment between the pack
  entry and the driver taps (the only part carrying motor current).

## Capacitors

Placement follows the two levels: the big cap goes **up on the shelf with
the ESP32**, the reservoir stays **down in the cavity with the driver**.

- **470-1000 µF electrolytic on the shelf board, where the twisted pair
  lands.** Motor starts and stalls yank the pack voltage down through the
  cells' internal resistance; this cap rides through the dip and stops it
  resetting the chip. It must sit at the ESP32 end of the run — the riser
  wires add resistance and inductance, and the cap only protects what's on
  its own side of them. Down in the cavity it would do half the job. Mind
  the polarity stripe.
- **100 µF electrolytic across the rails at the pack entry / driver taps, in
  the cavity.** A local reservoir for the motor current spikes, so they are
  served from millimetres away instead of travelling the strips. Polarity
  again.
- **2 off 100 nF ceramic, one across each motor's terminals.** Kills brush
  noise at the source; a different job from the electrolytics, not replaced
  by them.

## Wire list

| From | To | Wire | Notes |
|------|----|------|-------|
| Pack + / − | 6V / GND rails | red / black | enter next to the driver taps |
| 6V / GND rails | DRV8833 VIN / GND | red / black | immediately adjacent to pack entry |
| 6V / GND rails | ESP32 5V / GND pins | red / black | twisted pair up to the shelf; onboard LDO makes 3.3 V |
| ESP32 GPIO4 | DRV8833 AIN1 | green | left motor PWM |
| ESP32 GPIO5 | DRV8833 AIN2 | yellow | left motor PWM |
| ESP32 GPIO6 | DRV8833 BIN1 | red | right motor PWM |
| ESP32 GPIO7 | DRV8833 BIN2 | orange | right motor PWM |
| ESP32 GPIO10 | DRV8833 SLP | brown | drive high to enable; low = sleep |
| ESP32 3V3 | AMG8833 VIN | red | STEMMA QT colours shown |
| ESP32 GND | AMG8833 GND | black | to the ESP32 only — see Notes |
| ESP32 GPIO8 | AMG8833 SDA | blue | |
| ESP32 GPIO9 | AMG8833 SCL | yellow | |
| AMG8833 spare QT port | LSM6DSOX QT port | QT cable | I2C daisy chain, all four lines |
| LSM6DSOX spare QT port | INA219 QT port | QT cable | |
| Pack + (after switch) | INA219 VIN+ | red | shunt inserted in the pack + lead |
| INA219 VIN− | 6V rail entry | red | run both legs up/down as a twisted pair |
| ESP32 GPIO11 | GND | orange | wake LED + 1 kΩ series (~1.3 mA), on the shelf board: on = awake, off = asleep |
| DRV8833 AOUT1/AOUT2 | right motor | red / black | soldered with polarity inverted |
| DRV8833 BOUT1/BOUT2 | left motor | red / black | soldered with polarity inverted |

The motors ended up soldered crossed and inverted: the A channel drives
the right motor, the B channel the left, and both with inverted
polarity. The firmware compensates: the `MOTOR_x_IN1_GPIO`/
`MOTOR_x_IN2_GPIO` defines in `firmware_main.c` are deliberately
scrambled relative to the DRV8833 AIN/BIN pin names above (left = 7/6,
right = 5/4).

## GPIO summary

| GPIO | Function |
|------|----------|
| 4 | AIN1 (PWM) — right motor, inverted |
| 5 | AIN2 (PWM) — right motor, inverted |
| 6 | BIN1 (PWM) — left motor, inverted |
| 7 | BIN2 (PWM) — left motor, inverted |
| 8 | I2C SDA — QT chain: AMG8833 0x69, LSM6DSOX 0x6A, INA219 0x40 |
| 9 | I2C SCL — QT chain as above |
| 10 | DRV8833 sleep control |
| 11 | wake LED — orange, 1 kΩ to GND on the shelf: on = awake, off = asleep |
| 38 | onboard WS2812 RGB status LED — no wiring; DevKitC-1 v1.1 (v1.0 boards use 48) |

## Notes

- **Common ground.** The GND half of the twisted pair is the return for the
  five control signals between the shelf and the driver — no separate ground
  wire. Bundle the five signal wires loosely and route them down alongside
  the power pair.
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
- **USB without battery.** With the pack switched off, that same diode
  conducts the other way: USB 5 V lands on the 6V rail and powers the
  DRV8833 and motors too. Fine for console and sensor work, but don't
  run the motors like this — a stall pulls amps through a diode rated
  for about one, and through the host's USB port. The firmware guards
  this twice over: `drive()` checks the INA219 shunt current and, when
  the pack isn't supplying (near-zero reading), traces motor commands
  instead of executing them; and the DRV8833 auto-sleeps whenever both
  duties are zero. Still the habit: `s` before switching the pack off
  with USB attached — the guard blocks new commands, it can't stop
  motors already turning.
- **Noise.** Twist the wires to each motor; they stay inside the cavity, a
  level below the AMG8833's I2C run, which goes straight from the front
  standoffs up to the shelf without entering the cavity.
- **INA219 shunt.** The whole robot's current detours through the shelf-
  mounted INA219 (0.1R shunt: ~50 mV drop at cruise, ~0.3 V at a hard
  stall, on top of pack sag). Both legs of the detour run as one twisted
  pair so the motor spikes cancel; keep that pair clear of the QT cables,
  and suspect it first if thermal readings ever go jittery.
- **Sensor stays cool.** Mounting the AMG8833 outside on standoffs keeps it
  out of the cavity's warm air (motors and driver both dissipate) — a
  thermal camera reading its own waste heat sees a raised, flattened scene.
  If its view is clipped by the chassis lip, extend the standoffs rather
  than moving it inboard.
- **Antenna.** Orient the DevKit so its antenna end overhangs the shelf
  edge, pointing away from the battery below — the pack is the biggest RF
  obstruction on the robot, and f1 will want ESP-NOW eventually.
- **Strain relief.** The shelf-to-cavity runs flex when boards are lifted
  for battery swaps; leave slack in the twisted pair and signal bundle, and
  anchor them at both ends so the solder joints never take the load.
