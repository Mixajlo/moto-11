# 12. Power management: ignition-as-master-switch, RUNNING vs POWERED, battery backstop, charge warning

- Status: Accepted (refines the state logic in `electrical-plan.html` §13)
- Date: 2026-06-23
- Deciders: Miki

## Context
The original supervisor (electrical-plan §13) was a 4-state machine —
`SLEEP → ARMED → RUNNING → OFF_DELAY` — that left `RUNNING` **only** on
ignition-off. Bench testing against real riding scenarios surfaced gaps:

- **Stall / kill-switch:** engine stops but the key stays on. The rider wants to
  flip the kill switch / restart and carry on — accessories should **stay
  powered**, not cut.
- **Short stop (e.g. buying something), engine off, key left on for > 30 s:** the
  rider does **not** want the 30 s off-delay to cut power; they'll be back and
  restart with no interruption.
- **Engine-only loads:** heated grips shouldn't run when the engine isn't running
  (no charging) — so the firmware must distinguish "engine actually charging" from
  "key on, engine off."
- **Weak charging:** with several loads on, the bus can sit below a healthy charge
  voltage while the engine runs — the rider wants an early warning (this is a core
  reason the INA3221 sensing exists).

The original logic handled none of these: a stall left it stuck in `RUNNING`, and
the only exit was key-off → off-delay → cut.

## Decision
**The ignition key is the master switch.** Once powered, accessories stay on until
the key goes **OFF**. Engine-run (`Vbus ≥ V_RUN_ON`, debounced) only gates the
*first* power-on and distinguishes two live states. New machine:

`SLEEP → ARMED → RUNNING ⇄ POWERED`, plus `OFF_DELAY` on key-off.

- **RUNNING** — key on **and** engine running (charging); MASTER on. `engineRunning()`
  is true here only; engine-only loads (grips) gate on it.
- **POWERED** — key on, engine **off** (stall / kill / stop); MASTER **stays on** so
  a restart is seamless. Distinct from RUNNING so engine-only loads can shed.
- **Battery-save backstop** — if POWERED persists (engine off, key on) for a long
  time (default **10 min**, tunable; 0 = off), cut MASTER → ARMED to protect the
  battery from a forgotten key. Far longer than any normal stop, so it never bites.
- **OFF_DELAY** — entered only on key-off; holds MASTER for the delay (default 30 s)
  so a quick key-cycle/restart keeps power, then cuts and sleeps.
- **Charge-health warning** — while RUNNING, `Vbus` in `[12.8, 13.4) V` raises a
  (throttled) warning "loads too high or R/R weak", with a latched `chargeMarginal()`
  flag for a future tablet alert / automatic load-shed.

All thresholds and timers are tunables, intended to become tablet-configurable over
BLE (persisted to NVS) — see the roadmap.

## Consequences
- Behaviour matches real riding: stalls, kill-switch use, and short stops keep
  accessories powered; restart is seamless.
- The battery is still protected — by the long POWERED backstop (forgotten key) and
  the key-off off-delay → sleep — without nagging normal short stops.
- A clean `engineRunning()` signal lets load policy hold engine-only loads (grips)
  off when not charging. *(The actual grip-shed policy is a later `manageLoads()`
  step; the hook exists now.)*
- Early warning of marginal charging — the practical payoff of bus-voltage sensing.
- State machine grows 4 → 5 states; §13 pseudocode updated to match.
- Idle-sag risk: heavy load at idle can momentarily dip `Vbus`. Mitigated by
  hysteresis (`V_RUN_ON` 13.2 / `V_RUN_OFF` 12.9) plus run/stall debounces; the
  charge warning is informational and never cuts power.

## Alternatives considered
- **Keep the original §13 (cut on key-off only, no POWERED):** can't distinguish
  engine-running for load policy and gives no charge warning; also leaves a stall
  stuck in RUNNING. Refined by this ADR.
- **Strict engine-run gating (cut accessories whenever the engine stops):**
  rejected — it would cut power on every stall and short stop, rebooting the tablet
  and contradicting the rider's needs.
- **No backstop (trust the rider to kill the key):** rejected — a forgotten key
  would flatten the battery; the 10 min backstop is cheap insurance that never
  interferes with normal use.
