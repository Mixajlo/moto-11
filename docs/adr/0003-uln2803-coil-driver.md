# 3. ULN2803 coil driver; commoned pin 86 reused as +12, low-side on pin 85

- Status: Accepted
- Date: 2026-06-19
- Deciders: Miki

## Context
The pre-wired relay/fuse box has four relays whose coil **negatives (pin 86, black) come
commoned**, with the individual coil **positives (pin 85, white)** brought out separately.
The ESP must drive four coils individually from 3.3 V logic, and these relays have no coil
suppression of their own.

## Decision
Reuse the commoned **pin 86 as the +12 V coil rail** and **low-side switch each white/85 wire
to ground**, one channel per relay (coils are non-polarised, so swapping +/- is fine). Drive
them with a **ULN2803A Darlington array**: open-collector low-side sink, 3.3 V-logic inputs,
**active-HIGH** (GPIO high → output sinks the 85 line → relay pulls in). Tie the ULN's
**COM (pin 10) to the +12 rail** so its **internal clamp diodes handle flyback — no external
1N4007s**. A 10 kΩ pull-down on each input keeps relays off while the ESP boots.

## Consequences
- No need to cut/separate the factory coil common.
- No discrete flyback diodes on the box (built into the ULN via COM→+12).
- Firmware logic is active-high; relays only actuate when the box is powered (engine running).
- ~1 V Darlington drop → the coil sees ~11 V and still pulls in reliably.
- One chip can drive all coils, including the external master-relay coil.

## Alternatives considered
- **Discrete logic-level MOSFETs** (IRLZ44N / AOD4184 / AO3400): viable, but needs separating
  the coil common (or high-side drivers) plus an external 1N4007 per coil. More parts for the
  same result.
- **High-side switching the white/85 lines** as the box is wired for: rejected — needs a
  high-side driver stage per channel.
- **Non-logic-level MOSFET boards (IRF540/IRF520)**: rejected — won't fully turn on at 3.3 V.
