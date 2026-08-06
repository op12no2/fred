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
- **Performances** — `p a` I'm-alive wiggle, `p h` hello (the glimpse
  greeting: one gentle two-syllable swing, "hi-ya"), `p f` found-you
  shimmy; one function per performance, repertoire grows.
- **RGB status** — red boot/fail, green well, blue performing, violet
  while held. Nominal green is arousal-graded via `led_nominal()`: a
  dim ember while the watcher sleeps, full green awake — George read
  the watch toggle as waking him, and now the LED agrees (he also
  boots up asleep).
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
  separates them. A Foundation-style gesture command rides on it:
  **two brief lift-downs in quick succession toggle the watcher**
  (each under ~4 s, next lift within ~3 s) — blue-blue winks
  acknowledge "watching", one amber wink "not" — so an untethered f1
  needs no console to go on or off duty.
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
- **Coaxing, measured** (gestures.log — acts clapperboarded by
  lift/down `held` blips): against a per-pixel background a *standing*
  person at 2 m reads +3.6–4.1 °C (double the old frame-median
  estimate); standing at 3 m is marginal (+1.3) but **crouching** at
  3 m reads +2.0 — and at 1 m fills 14 pixels vs ~5 standing, since
  standing close shows him only legs. Walk-pasts spike hard (+5.8) but
  dwell 1–3 s; every coaxing act sustained 76–80% presence — **dwell
  ≥ ~5 s separates transit from visit** with no overlap. Approach and
  retreat are clean monotonic blob-size ramps. Response probability ∝
  blob size × dwell falls straight out.
- **The stationary room** (quiet_room_sat.log, 4 humanless minutes):
  per-pixel noise ~0.3 °C; frame median steady to ±0.2 °C; the worst
  any pixel strayed above its own long mean was 1.45 °C, so **a
  per-pixel background + 1.5 °C threshold gave zero false alarms** —
  and a walking human registers +7 °C against it. Background settles
  in ~10 s of stillness. The spinning room (quiet_room_spin.log) has
  stable heading structure — conservatory sector +0.8 °C over the hall
  side — and even at 59 dps only 1% of frames grazed the old
  frame-median +2.0 bar.

## The watcher

**v1 heartbeat implemented** (`w` toggles; every knob a `WATCH_*`
define citing f1/log/): rest with the driver asleep, learning a
per-pixel background (seeded after every move or set-down, trusted
after ~10 s, updated only on quiet frames so a visitor can't become
wall) → look around at log-normally random intervals (median 3 min
fresh, stretched ×3 as the resting-voltage mood tires, clamped
30 s–30 min) → the look is a gyro-metered full circle whose speed
falls with mood and whose **gaze lingers** — passing warmth above the
scene mean sheds sweep duty down to a crawl. Nudges pull the next look
to ~20 s: a glimpse (any pixel > background +1.5 °C, the zero-false-alarm
threshold) or a set-down ("new spot"). Held aborts everything, as
ever. Rest = green, looking = blue, and a glimpse on arrival earns a
beat of **amber** ("interesting") and — once per rest, re-armed by
each look, or he's very predictable — a **hello** (`p h`, a gentle
two-syllable swing — the coaxing precursor), after which the
background reseeds (the wiggle moved the eye a little) and the
visitor waits as wallpaper for the look. Later glimpses in the same
rest get the amber beat only, still updating the remembered angle
and pulling the look closer: noticed, said nothing. And sometimes
the hello is **withheld anyway** — 15% fresh, rising to 60% on a
flat pack — and the shrug spends the rest's hello, so greeting is
never a certainty and tiredness reads as standoffishness. The look now **ends facing the best thing it
showed**: the sweep remembers the yaw of peak gaze-drag and turns
back to it by the shortest way round; if nothing rose above the gaze
deadband the whole circle, it turns instead to the remembered angle of
the glimpse that called the look (degrees off the measured 3.2
boresight, mirrored sign as field-tested); if there was neither, he
stops where the circle ended. The spin itself teaches which drive
sign yaws which way, so the settle turn needs no calibration
constant. Then reseed and rest as usual. The full look stays deferred
and jittered (every "soon" is `WATCH_SOON_S` ± half); transitions
print with mood and volts; `ws` column (1 rest, 2 look, 3 reorient)
in the recorder.

**The rhythm**: deep sleep from power-on until the first wake — the
gesture is half the fun — then f1 lives an asymmetric sleep/wake
cycle, a fresh span drawn log-normally at *every* transition, manual
or autonomous alike: awake median 2 h fresh, **shrunk** ÷3 by
tiredness; asleep median 1 h fresh, **stretched** ×3 (sigma 0.5,
clamped 20 min–6 h and 15 min–4 h). So waking a sleeping f1 grants a
full awake span and putting him to sleep earns a full sleep — the
gesture genuinely matters, it never just pauses a timer. And the day's
content gets a vote on bedtime: each wake grants two finite pots, a
**second wind** (45 min fresh, ÷3 tired) that interesting events — a
glimpse at rest, a gaze the look confirmed — spend pushing sleep later,
and a **nothing doing** pot (30 min fresh, ×3 tired) that looks finding
nothing at all spend pulling it closer. Each event takes a fixed
fraction of its pot's *remainder*, so the first glimpse of the evening is
worth the most and the effect saturates geometrically — a busy room
can't keep him up forever, and the same child genuinely can't keep a
tired f1 up as long. Because glimpses are edge-triggered and the
background reseeds after every look, parked warmth becomes wallpaper
and stops paying: only *change* keeps him up. Staying up later drains
the pack, which shrinks tomorrow's wind pot and stretches sleep — a
big evening earns a long lie-in with no code saying so. Asleep is
watcher-off: the ember LED, with pickup and the gesture still
listening — sleep stays sacred, no glimpse shortens it, or he was never
really asleep and the wake gesture would cheapen. He won't cycle while held; "watch: nodding off" / "waking
by himself" on the console, and each toggle prints its drawn span.
Power-cycling returns him to deep sleep — the honest meaning of a
power cycle.

**Next layer — responding**: the hello is the down-payment on this.
Still to come: double-check (persistence against the background),
then with probability ∝ blob size × dwell (gestures.log) a tentative
step or two toward the visitor, each ending in rest and
re-observation.

## Data recorder

Tuning happens against logs, not memory. `r` starts/stops a 10 Hz
recording of everything — thermal frame, gyro/accel, volts/amps,
commanded motor duties, held state — into a PSRAM ring (~1 h
capacity); `d` dumps it as CSV over the console. Columns:
`ms,left,right,volts,ma,held,gx,gy,gz,ax,ay,az,p0..p63`.

Runs worth keeping move to `f1/log/` with descriptive names
(`quiet_room_spin.log` — the 32-lap empty-room thermal panorama;
`quiet_room_sat.log` — the stationary background/noise run;
`gestures.log` — walk-pasts, standing/crouching range ladders and a
retreat/approach, acts delimited by lift/down blips in the `held`
column); throwaway dumps stay untracked in `firmware/` as
`log.firmware.<timestamp>.txt`.

Two wrinkles in retrieving a run, both monitor-side. PSRAM is wiped by
reset, and `idf.py monitor` resets the chip on connect — always attach
with `idf.py monitor --no-reset` (monitor.sh does). And `d` only writes
to the serial wire; the file is made by whatever is listening, so
either toggle the monitor's file logging with Ctrl+T Ctrl+L around the
dump, or start it as `idf.py monitor --no-reset -l run.log` and the
whole session lands in the file. On the bench:
`pandas.read_csv(path, comment="#")`, trimming any non-CSV monitor
chatter first.
