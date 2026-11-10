# 19. Load manager: engine-only grips, grips → fog shed ladder, crank hold

- Status: Accepted (implements the `manageLoads()` hook left by ADR-0012)
- Date: 2026-11-10
- Deciders: Miki

## Context
ADR-0012 split RUNNING (charging) from POWERED (key on, engine off) and added a debounced
`chargeMarginal()` flag. It left the load policy itself as a later `manageLoads()` step. Since
ADR-0004 and ADR-0018, the only loads the ESP may shed are the **grips and then the fog**.
Headlights are off the ESP, and MASTER belongs to the supervisor.

Four needs shaped the design:
- **Grips are engine-only.** About 4 A with no charging flattens the battery, so grips must
  not run in POWERED or ARMED. A rider who presses the grip button before starting the engine
  still means "grips on once it runs".
- **Marginal charging** at idle with fog and grips on (about 8 A against the idle surplus)
  should shed load in priority order. It must not oscillate, and it must undo itself once
  charging recovers.
- **Cranking:** the starter wants the whole battery. The factory already cuts the headlights
  while START is held, and the ESP has START sensed on PC817 channel 2 (GPIO 35).
- **Bench tools must keep working.** The relay walk-through and bike self-test switch relays
  directly from the console, in SLEEP or ARMED.

## Decision
We will add a `LoadManager` (`src/loads.{h,cpp}`) between rider intent and the managed
relays.

- **Wanted vs allowed.** Buttons and the console command `loads want` call
  `request(relay, on)`, which records what the rider *wants*. The manager drives each relay
  to *wanted AND allowed*. A blocked request is remembered and comes on by itself when it is
  allowed again. A momentary button toggles the *wanted* state, so pressing it while a load
  is held off cancels the request. SPARE and AUX pass straight through. MASTER requests are
  ignored.
- **Block reasons** (bit flags; the console command `loads` shows them):
  - `engine-not-running`, grips only: the supervisor is not in RUNNING.
  - `charging-marginal`: shed by the ladder.
  - `cranking`: START held, plus `crankHold` (default 1.5 s) after release. `crankHold 0`
    turns crank shedding off.
- **Shed ladder**, judged only while RUNNING:
  - **Shedding:** on the marginal edge, shed the next load in the order **GRIP, then FOG**
    that is actually on; loads that are off are skipped. While still marginal, shed one more
    load every `shedStep` (default 30 s).
  - **Restoring:** after recovery, wait one `shedStep`, then restore one load per `shedStep`
    in reverse order (**FOG first, then GRIP**), skipping loads that are no longer wanted.
  - **Leaving RUNNING** clears the ladder. A stall is then the rider's choice (ADR-0012),
    and grips are held off by the engine-only rule anyway.
- **Edge-driven relays.** The manager writes a relay only when the allowed state changes,
  so a raw console `on/off` is a bench bypass. One exception: if a **new** block reason
  appears while a managed relay is physically on, it is shed and marked wanted, so it is
  restored with the rest.
- **Inputs as a plain struct.** `main.cpp` fills `LoadInputs` from `Engine` and `Sensor`.
  The manager has no supervisor dependency and is unit-tested on the host
  (`test_host/test_loads_native.cpp`).
- **Tunables:** `shedStep` (5–600 s) and `crankHold` (0–10 s) become runtime config keys
  (ADR-0013); they are additive, so there is no version bump.

## Consequences
- Grips can't drain the battery with the engine off, and a pre-start grip press "just works".
- Marginal charging now acts, not only warns. The dwell, the restore delay and the upstream
  5 s debounce on the marginal flag keep it from flapping (each cycle is ≥ 35 s).
- The relay walk-through and bike self-test are unaffected. Their raw console commands run
  in states where no *new* block reason appears. One exception: on the bike, a real marginal
  condition during the grips step will shed the grips, which is the correct behaviour.
- Crank shedding depends on GPIO 35 really seeing "START held". The bike's START wire is
  still to be confirmed (live **only while START is held**). If the wrong wire is used,
  "cranking" reads true all the time and holds fog and grips off. Set `crankHold 0` until
  the wire is verified on the bike.
- Follow-ons: a POWERED low-battery floor (e.g. shed the fog below about 11.8 V); tablet
  display of block reasons; v2 PWM could dim instead of shedding.

## Alternatives considered
- **Continuously enforce the policy every loop** (relay = allowed, always). Rejected: it
  breaks the console relay test and the bike self-test, which run with the engine off.
- **Shed everything at once on marginal.** Rejected: the grips alone usually fix idle
  charging, and dropping the fog without need costs visibility.
- **Restore both loads at once on recovery.** Rejected: it re-adds the full load in one
  step and invites a marginal ↔ recovered oscillation.
- **Refuse a grip request with the engine off** (forget it). Rejected: riders press the
  grips before starting, so remembering the request is the expected behaviour.
