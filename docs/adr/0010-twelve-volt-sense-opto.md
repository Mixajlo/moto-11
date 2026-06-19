# 10. 12 V sense front-end: PC817 opto-isolation over a resistor divider

- Status: Accepted
- Date: 2026-06-19
- Deciders: Miki

## Context
The ESP must read two 12 V logic signals — **ignition/run-sense** (also a deep-sleep `ext0`
wake source) and **start-button / headlight-feed sense**. The project docs disagreed on how:
`electrical-plan.html` Fig 5.1 shows a **resistor divider (100k/27k) + 3.3 V Zener clamp + RC
filter** into the GPIO, while `CLAUDE.md` specifies a **PC817 opto-isolator**. Both land a safe
level on the pin, so the question is which is the better front-end for a motorcycle's 12 V bus —
which is electrically hostile: load-dump, relay/coil inductive kickback, and alternator ripple.
The chosen sense pins (GPIO34/35) are also **input-only**, with no internal pull-ups and only
pad-level protection.

## Decision
We will use a **PC817 optocoupler per channel** (U4 = ignition, U5 = start/headlight). The 12 V
side drives the opto LED through a series resistor (~2.2 kΩ, ~5 mA); the output transistor pulls
the GPIO to GND through a 10 kΩ pull-up to 3.3 V. **Logic inverts: 12 V present → GPIO LOW.**
Firmware reads the senses with that inversion. The resistor-divider+Zener remains documented as
a drop-in alternative if a future board wants fewer parts.

## Consequences
- Strong transient immunity: bus spikes are absorbed by the opto LED / series resistor, not
  coupled into the input-only GPIO.
- Current-loop signalling gives clean, debounced edges — good for the `ext0` ignition wake.
- Firmware must invert the two sense inputs (trivial); document it at the read site.
- PCB cost: two optos + four resistors, and a **≥ 4 mm isolation gap** must be kept clear under
  U4/U5 (no ground pour across the barrier) — captured in the mainboard design rules.
- Galvanic isolation is partly moot (ESP and bike share ground); the win here is robustness, not
  isolation per se.

## Alternatives considered
- **Resistor divider + Zener clamp + RC** (electrical-plan Fig 5.1): fewer/cheaper parts and
  non-inverted, but directly couples the noisy bus to a sensitive input-only pin and leans on a
  small Zener to survive repeated transients. Kept as the documented fallback.
- **Divider into a protected GPIO with a series resistor only**: simplest, but least robust to
  load-dump — rejected for an always-connected automotive device.
