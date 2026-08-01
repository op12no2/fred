# f0

f0 is the fleet's ancestor: A machine with exactly one idea. A giant red arcade button rides
on top. Press it and f0 spins in place; release it and he winds down
slowly, like a music box, glowing button fading with him.

f0 is entirely analog; a resistor and two supercapacitors:

- Press: the motors run straight off the AA pack while a 5F supercap bank
  charges through a power resistor.
- Release: the motors (and the button's LED) run on from the caps, sagging
  gracefully to a stop.
- Because the bank takes seconds to charge, **the length of the press sets
  the length of the dance**. Nobody has to be told this; it gets
  discovered.

The two motors are wired in opposite polarity, so all he ever does is
pirouette.

## Docs

- [Parts](./doc/parts.md)
- [Schematic](./doc/schematic.svg)

## Tuning

The series power resistor is the one knob — bigger = gentler
start and shorter fade. Start at 4.7R, drop toward 2.2R for a longer
wind-down.
