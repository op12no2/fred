# f0 — the button-bot

f0 is the fleet's ancestor: f1's original chassis, motors and caster,
reborn as a machine with exactly one idea. A giant red arcade button rides
on top. Press it and f0 spins in place; release it and he winds down
slowly, like a music box, glowing button fading with him.

**The rule: no chips, no code.** f0 is entirely analog — the "firmware"
is a resistor and two supercapacitors:

- Press: the motors run straight off the AA pack while a 5F supercap bank
  charges through a power resistor.
- Release: the motors (and the button's LED) run on from the caps, sagging
  gracefully to a stop.
- Because the bank takes seconds to charge, **the length of the press sets
  the length of the dance**. Nobody has to be told this; it gets
  discovered.

The two motors are wired in opposite polarity, so all he ever does is
pirouette. The 1:90 gearboxes that proved too slow for f1 make it stately.

- [Parts](./doc/parts.md)
- [Schematic](./doc/schematic.svg)

Tuning: the series power resistor is the one knob — bigger = gentler
start and shorter fade. Start at 4.7R, drop toward 2.2R for a longer
wind-down. Mind the supercap polarity and the per-cap balance resistors.

## Future

The no-code rule is load-bearing, but one evolution is permitted in
spirit: f0 could someday *say hi* to the fleet without ever listening — a
tiny self-contained beacon (e.g. a coin-cell ESP32-C3 taped on like a
voice box) shouting a presence packet on the fleet's ESP-NOW channel when
the button is pressed. The analog robot underneath would never know.
