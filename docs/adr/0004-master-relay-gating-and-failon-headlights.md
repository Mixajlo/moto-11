# 4. External master relay gates the box; headlights stay outside it (fail-ON)

- Status: Accepted
- Date: 2026-06-19
- Deciders: Miki

## Context
The combo box has four fused relay outputs plus **two fuses (5 A and 30 A) that feed straight
out, bypassing the relays**. We want the whole box gated to the engine-running state, and we
need lighting that behaves safely if the controller fails.

## Decision
Feed the box's main 12 V through an **external, ESP-driven master relay** that closes only on
the engine-run signal. This hardware-gates the entire box and turns the 5 A / 30 A direct-outs
into **engine-switched** feeds. The **ESP draws its always-hot supply from its own battery tap
(inline 2 A fuse), upstream of the master relay**, so it outlives the relay for the off-delay
and parked sentinel. **Headlights stay OUTSIDE the gated box (fail-ON):** their relay coils are
triggered from the original start-button feed (relieving the starter switch and preserving the
factory cranking cut); one bulb is always-on, the second on a **5-pin NC relay** the ESP
energises only to shed in a critical low-power moment.

## Consequences
- Hardware engine-gate for the whole box → the four internal relays only do user on/off, no
  per-relay firmware engine check.
- The 5 A / 30 A direct-outs become useful switched feeds (e.g. tablet buck, a switched sub-bus).
- ESP power **must** be a separate battery tap, not the box.
- Fail-safe directions are deliberate: accessories **fail-OFF** (safe), headlights **fail-ON**
  (safe). Load-shed priority: grips → fog → (last, daylight only) one headlight; never all
  forward light.

## Alternatives considered
- **Firmware-only gating, per relay**: rejected — cannot gate the 5 A / 30 A direct-out fuses.
- **Headlights inside the gated box**: rejected — an ESP or master-relay fault would leave the
  bike dark; safety-critical lighting must never be solely behind the controller.
