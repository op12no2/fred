# f1 — firmware design

Direction change (2026-08-04): the heat-seeking missile is dead, long
live the wanderer. Live trials showed SEEK/TRACK/ARRIVE asking too much
of an 8×8 camera and two unmatched TT motors; the hunt machinery
worked, taught us everything below, and was removed (it came and went
between commits, so its legacy is this file, not git history).

The behaviour goal is now: **f1 wanders fairly aimlessly with a slight
penchant for heat** — settles in warm places, wanders off occasionally,
responds happily when someone comes to see him (heat transients), and
is livelier on fresh batteries. Vastly simpler, still fun. Build order:
get the primitives trustworthy first, then compose the wanderer from
them.

## Primitives (implemented, in `firmware_main.c`)

- **Motor layer** — `drive(left, right)` with L/R trim, deadband remap,
  and the USB-power guard (see schematic.md).
- **Performances** — `p a` I'm-alive wiggle, `p f` found-you shimmy;
  one function per performance, repertoire grows.
- **RGB status** — red boot/fail/bump, green well, blue performing.
- **Recorder** — `r`/`d`, 10 Hz all-sensor CSV (see below).
- **Calibration** — `c [secs]`, scripted open-loop motor runs.
- **Bump/stuck detection** — every tick while motors are commanded: a
  jolt (accel deviation > `BUMP_JOLT_G`) means he hit something; low
  gearbox vibration (< `STUCK_VIB_G`) means he's commanded but not
  moving. Logged per tick in the `bump` column, red LED flash on
  event. Thresholds are first guesses — tune from recorded logs of
  deliberate collisions before the wanderer relies on them.
- **Escape reflex** (`b` toggles, default on; the cal script disables
  itself) — on bump/stuck: reverse 1 s (or forward, if he was
  reversing), gyro-measured about-turn of 180±30° in a random
  direction (exact 180s ping-pong in corners), then resume the
  pre-bump command. Bumps within ~3 s of an escape count as still
  trapped; after 4 tries he stops rather than thrash.

## Measured facts (hard-won, the rebuild stands on these)

- A person at 2 m reads **+2.0–2.4 °C** over ambient (21 °C room).
  Working detection: acquire at ambient +2.0, keep at +1.0, 4-frame
  same/adjacent-column persistence, minimum blob weight so one noise
  pixel can't steer. Pixel noise is real: ±2.5 °C.
- **Movement, not temperature, tells people from furniture.** Liveliness
  (EMA of per-frame centroid motion): people 0.03–0.07, a hot window
  0.007 — and the window ran *hotter* than the person (+7 °C vs +3.4).
- **Ambient baseline**: gated EMA of the frame median — learn only while
  coverage < 30%, else an approaching target drags its own reference up.
- **Boresight**: dead ahead is image column ~3.2, not the geometric 3.5.
- **Trim**: straight needs left − right ≈ −1, set open-loop (`c` runs) —
  never fit trim from behaviour logs, feedback biases the fit.
- **Deadband**: dead at 20% duty, reliable from rest at 25%, one-wheel
  random breakaway below; `motor_set` remaps commands onto 25–100%.
- **Vibration** is a clean moving/stalled witness: <2 mg at rest,
  >29 mg rolling.
- **Gyro**: z bias −0.7 dps at rest; handling (pickup) is a 150:1
  signal — pickup detection is nearly free when wanted.

## The wanderer (to design)

A mood/impulse loop over the primitives, not a state machine with a
goal: mostly amble (short arcs, pauses), linger where the frame is
warm, occasionally get restless and move on, greet a heat transient
(someone arriving) with `p f`, back off and turn on bump/stuck. Energy
budget scales with pack voltage — livelier on fresh batteries, sleepy
on flat ones. Design properly once bump detection is trusted.

## Data recorder

Tuning happens against logs, not memory. `r` starts/stops a 10 Hz
recording of everything — thermal frame, gyro/accel, volts/amps,
commanded motor duties, bump events — into a PSRAM ring (~1 h
capacity); `d` dumps it as CSV over the console. Columns:
`ms,left,right,volts,ma,bump,gx,gy,gz,ax,ay,az,p0..p63`.

Two wrinkles in retrieving a run, both monitor-side. PSRAM is wiped by
reset, and `idf.py monitor` resets the chip on connect — always attach
with `idf.py monitor --no-reset` (monitor.sh does). And `d` only writes
to the serial wire; the file is made by whatever is listening, so
either toggle the monitor's file logging with Ctrl+T Ctrl+L around the
dump, or start it as `idf.py monitor --no-reset -l run.log` and the
whole session lands in the file. On the bench:
`pandas.read_csv(path, comment="#")`, trimming any non-CSV monitor
chatter first.
