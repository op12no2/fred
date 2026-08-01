# Fred — heat-seeking firmware design

Where the firmware is today: `firmware_main.c` initialises the AMG8833 and
prints the mean of all 64 pixels once a second. No motor code yet. This doc
is the design for the rest, and the list of wrinkles that need solving on
the way.

## Sensor facts that shape the design

- 8x8 pixels spanning a 60° cone (~7.5° per column), 10 fps, 0.25 °C per
  LSB, and — the big one — **±2.5 °C noise per pixel**.
- A pixel reads the area-weighted average of everything in its view. A
  person 3-4 m away part-fills one pixel, so they show up as a single
  column maybe only 2-3 °C above background: one red blob on the blues,
  barely above the noise. All the marginal-signal wrinkles below stem from
  this.
- The sensor is mounted behind an open vertical window, recessed ~3 mm with
  ~4 mm clearance either side — the 60° cone clears the frame, no
  vignetting.

## The algorithm

Two classical ideas, combined. The naive plan (rotate, differentiate heat
over the sweep, advance if improving) is **run-and-tumble** — how bacteria
climb gradients with a one-pixel sensor. But the camera's 8 columns deliver
a spatial gradient across 60° in every frame, so the inner loop can instead
be **centroid steering** (visual servoing in its simplest form; also
Braitenberg vehicle 2b): steer continuously toward the heat, no
stop-rotate-sample phases.

Per frame in TRACK:

1. Sum each column's above-threshold heat.
2. Weighted centroid column `c` (a float, 0..7).
3. Steering error `e = c - 3.5`.
4. `left = base + k*e`, `right = base - k*e` — Fred arcs toward warmth.

Wrapped in a small state machine:

| State | Behaviour | Exit |
|-------|-----------|------|
| SEEK | spin slowly in place | blob acquired → TRACK |
| TRACK | centroid steering, forward | coverage high → ARRIVE; blob gone ~1 s → SEEK |
| ARRIVE | stop, bask | blob shrinks/moves → TRACK |

Centroid degradation is graceful by construction: at range the error
quantises to 7.5° column steps (coarse, slightly bang-bang); as the blob
grows to span columns, the weighted centroid interpolates *between* columns
and steering smooths out. Coarse when far, fine when near — and the far
regime is self-curing, because approach rapidly improves pixel fill and
signal. Only acquisition is hard.

## Wrinkles to solve

Signal (the marginal single-pixel target):

- **Ambient baseline.** "Above threshold" means above ambient, which
  varies. Options: the frame median (most pixels are background), and/or
  the AMG8833's onboard thermistor register (0x0E). Frame median is
  probably the more honest baseline; decide empirically.
- **Hysteresis.** One threshold to *acquire* a target (high, e.g. ambient
  +3 °C), a lower one to *keep* it — otherwise a marginal pixel flickers
  Fred between SEEK and TRACK every frame.
- **Persistence.** A single noise pixel is indistinguishable from a distant
  person in one frame. Promote a blob to "target" only if something shows
  in the same or an adjacent column for ~3 consecutive frames. Noise
  doesn't repeat in place; people do.
- **Smoothing.** EMA on the centroid (or the frame) to stop whole-column
  noise lurches steering Fred drunk. Costs reaction lag, which he can
  afford.
- **LOST grace.** Blob gone → keep last heading for ~1 s before reverting
  to SEEK, so a two-frame dropout doesn't restart the spin.

Movement:

- **SEEK spin rate vs frame rate.** Slow enough that 10 fps gives 2+
  frames per 60° of rotation, so nothing slips between glances.
- **ARRIVE criterion.** "Image fills the frame": fraction of pixels above
  threshold (say >60% coverage), plus a max-temp floor so a sunlit patch
  doesn't qualify. Needs tuning against a real person/radiator/cat.
- **Column↔direction mapping.** Establish empirically which end of a row is
  Fred's left — depends on sensor mounting orientation. Get the sign of
  `k` wrong and Fred flees heat (Braitenberg's "fear" vehicle — funny
  once).
- **Motor calibration.** TT motors have a PWM deadband (below some duty
  they hum, not turn) and no two are matched — find the deadband and the
  L/R trim for driving straight, over the USB tether. `base` and `k` tune
  after that.
- **nSLEEP discipline.** Hold GPIO10 low until the firmware means to move.
  Also the operating rule: pack on before motors run — with the pack off,
  motor current would be pulled from USB (see schematic.md).

## Tuning parameters (expected)

`acquire/keep thresholds, persistence frames, EMA alpha, base speed, k,
SEEK spin duty, ARRIVE coverage + temp floor, LOST grace time, L/R trim,
PWM deadband` — all found over the USB tether with a human test subject.
