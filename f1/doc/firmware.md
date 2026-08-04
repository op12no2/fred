# f1 — firmware design

The concept (2026-08-04, third and settled): **the watcher**. f1 is
mostly stationary — motor driver asleep, eyes open. Now and again
(scaled by pack voltage: frisky on fresh cells, sleepy on flat ones) he
looks around: a spin or two. A new, compact source of heat — not
full-frame, that's someone already on top of him — earns a double-check
and perhaps a tentative move toward it, probabilistic but weighted by
signal strength, so a child coaxing harder genuinely raises the odds.
When it works they'll believe they coaxed him, and they'll be right.

Why stationary: every failure of the abandoned heat-seeking hunt was
motion-induced (spin smear, veer, baseline churn) and every success was
at rest. Stillness buys sensitivity — a stationary f1 can learn a
per-pixel background of his corner of the room and detect *change*
against it, far sharper than the absolute thresholds motion forced. And
stillness buys hours: watching costs ~50 mA, driving 200–800.

No obstacle avoidance. It was a trick played on the sensors, and the
watcher doesn't need it: moves are short, tentative, and end in
stillness. The gyro's real gift is knowing when he's been picked up.

## Primitives (implemented, in `firmware_main.c`)

- **Motor layer** — `drive(left, right)` with L/R trim, deadband remap,
  the USB-power guard (see schematic.md), and DRV8833 auto-sleep: the
  driver is awake only while a wheel is turning.
- **Performances** — `p a` I'm-alive wiggle, `p f` found-you shimmy;
  one function per performance, repertoire grows.
- **RGB status** — red boot/fail, green well, blue performing, violet
  while held.
- **Pickup detection** — while idle, sustained rotation (>20 dps,
  3 ticks) means he's in someone's hands (handling is a 150:1 signal);
  while driving, sustained tilt (az < 0.90, ~25°) is the witness
  instead, since rotation is then normal and the habitat's floors are
  flat. The tilt witness is deliberately broader than hands: it also
  fires when he's climbing an obstacle, teetering on an edge, or
  falling (free-fall reads az → 0) — any "ground no longer properly
  under me" kills the motors (and so sleeps the driver) within
  ~300 ms, and the pre-pickup command is not restored on set-down, so
  a rescued or landed f1 stays put rather than lunging off with stale
  orders. A quiet second means set down. `held` column, "picked up!" /
  "set down" on the console, violet LED. Validated idle-side (log
  20260804161731): 3 pickups detected, 17 knocks up to 148 dps
  rejected — knock magnitude out-spikes carries, only duration
  separates them.
- **Recorder** — `r`/`d`, 10 Hz all-sensor CSV (see below).
- **Calibration** — `c [secs]`, scripted open-loop motor runs.

Console: `m s t v g c p l r d ?`.

## Measured facts (hard-won, the watcher stands on these)

- A person at 2 m reads **+2.0–2.4 °C** over ambient (21 °C room).
  Working detection: acquire at ambient +2.0, keep at +1.0, 4-frame
  same/adjacent-column persistence, minimum blob weight so one noise
  pixel can't steer. Pixel noise is real: ±2.5 °C.
- **Movement, not temperature, tells people from furniture.** Liveliness
  (EMA of per-frame centroid motion): people 0.03–0.07, a hot window
  0.007 — and the window ran *hotter* than the person (+7 °C vs +3.4).
- **Ambient baseline**: gated EMA of the frame median — learn only while
  coverage < 30%, else an approaching target drags its own reference
  up. (The stationary watcher can do better: per-pixel background.)
- **Boresight**: dead ahead is image column ~3.2, not the geometric 3.5.
- **Trim**: straight needs left − right ≈ −1, set open-loop (`c` runs) —
  never fit trim from behaviour logs, feedback biases the fit.
- **Deadband**: dead at 20% duty, reliable from rest at 25%, one-wheel
  random breakaway below; `motor_set` remaps commands onto 25–100%.
- **Current**: idle ~50 mA, cruising 160–190, leaning on a wall or
  stalled 700–825 — current is the honest witness of contact if ever
  needed again.
- **Vibration**: <2 mg at rest, >29 mg rolling. **Driving vibration**
  reaches p99 0.24 g at 10 Hz on tile, which is why gentle collisions
  are invisible to the accelerometer.
- **Gyro**: z bias −0.7 dps at rest; handling (pickup) is a 150:1
  signal.

## The watcher (to design)

A quiet loop over the primitives: rest (driver asleep, frame watched,
per-pixel background learning) → occasional look-around, rate scaled by
resting pack voltage → on a new compact warm blob: double-check
(persistence against the background), then with signal-weighted
probability a tentative step or two toward it, each ending in rest and
re-observation → greet a confirmed close visitor (`p f`) → picked up:
violet, motors stay silent, enjoy the ride. First design questions: how
long a settled frame needs to become a trusted background, and what
"new blob" means against it.

## Data recorder

Tuning happens against logs, not memory. `r` starts/stops a 10 Hz
recording of everything — thermal frame, gyro/accel, volts/amps,
commanded motor duties, held state — into a PSRAM ring (~1 h
capacity); `d` dumps it as CSV over the console. Columns:
`ms,left,right,volts,ma,held,gx,gy,gz,ax,ay,az,p0..p63`.

Two wrinkles in retrieving a run, both monitor-side. PSRAM is wiped by
reset, and `idf.py monitor` resets the chip on connect — always attach
with `idf.py monitor --no-reset` (monitor.sh does). And `d` only writes
to the serial wire; the file is made by whatever is listening, so
either toggle the monitor's file logging with Ctrl+T Ctrl+L around the
dump, or start it as `idf.py monitor --no-reset -l run.log` and the
whole session lands in the file. On the bench:
`pandas.read_csv(path, comment="#")`, trimming any non-CSV monitor
chatter first.
