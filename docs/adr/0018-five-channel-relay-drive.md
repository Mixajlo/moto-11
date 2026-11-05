# 18. Five-channel relay drive: J2 = master relay, J3 = four box relays, AUX on GPIO 12

- Status: Accepted
- Date: 2026-11-05
- Deciders: Miki
- Refines: [ADR-0003](0003-uln2803-coil-driver.md), [ADR-0004](0004-master-relay-gating-and-failon-headlights.md), [ADR-0009](0009-gpio-pin-map.md), [ADR-0017](0017-uln-com-on-always-hot.md)

## Context
The v1 mainboard drove four ULN channels: MASTER, FOG, GRIP and SPARE, all on the 4-pin J3. Two
things didn't fit the real hardware:

1. **The relay box has four relays, and the master relay is a fifth, separate relay**
   ([ADR-0004](0004-master-relay-gating-and-failon-headlights.md)). With MASTER on J3.1, one box
   relay had no driver.
2. **J2 carried `COIL_RAIL` + GND as an external jumper** to always-hot
   ([ADR-0017](0017-uln-com-on-always-hot.md)). The GND pin had no real use, and the master coil +
   needed its own wire to always-hot anyway.

The headlight relay was also on the list for a ULN channel (an ESP-energised NC "shed" relay).
That puts a forward light behind the ESP, against the fail-ON rule, so it stays off the ULN (see
Decision).

## Decision
- **ULN ch1 → J2 (master relay).** J2.1 = `+12V_PROT` (master coil + / 86, always-hot),
  J2.2 = `COIL_MASTER` (coil 85). The 2-pin connector is the whole master relay coil.
- **ULN ch2–5 → J3 (relay box).** J3.1 FOG, J3.2 GRIP, J3.3 SPARE, J3.4 **AUX**: one pin per box
  relay's white pin-85 wire. Box pin 86 stays on the box's own gated supply (ADR-0017).
- **ch5 = `AUX_EN` on GPIO 12**, with a 10 kΩ pull-down (R23) like R1–R4.
- **ULN COM tied to `+12V_PROT` on-board.** The `COIL_RAIL` net and its external jumper are gone.
  `+12V_PROT` is always-hot (after the reverse-polarity FET on the fused battery tap), so
  ADR-0017's split still holds.
- **ch6–8 unused:** inputs to GND, outputs unconnected.
- **The headlight relay is not on the ESP at all.** It is a 5-pin relay whose coil is driven from
  the start-button signal: bulbs on 87a (NC), 30 on an ignition-switched fused feed. Pressing
  the starter opens 87a and drops the bulbs (the factory cranking cut); otherwise they are lit.
  It can be mounted up front near the button. No ESP, no ULN, fail-ON by construction.

## Why GPIO 12 is acceptable here
ADR-0009 rejected strapping pins for relay enables. GPIO 12 (MTDI) sets the flash voltage at
reset: **LOW = 3.3 V flash**, which is what the WROOM-32D needs. A HIGH at reset would select
1.8 V and the module would fail to boot.

- R23 pulls it **down**, so at reset it reads LOW: the correct strap and the relay OFF. The
  strap and fail-OFF want the same level.
- The ULN input itself also loads the pin toward GND.
- Firmware drives it HIGH only after boot. On any reset (watchdog, OTA, brown-out) the pin
  returns to input, R23 pulls it LOW and the strap reads correctly again.
- **R23 must never become a pull-up**, and nothing else may drive GPIO 12 HIGH during reset.

GPIO 12 was the free output-capable pin left after the eight buttons (ADR-0014), so the
alternative was dropping a button input.

## Consequences
- All four box relays plus the master are now driveable. Firmware gains a fifth relay channel
  (`aux`), selectable in the console and in `btn map`.
- Load-shed is grips → fog only. With the headlight off the ESP there is no "shed one
  headlight" step; forward lighting never depends on the ESP.
- One fewer field wire: no `COIL_RAIL` jumper. The master relay is wired entirely from J2.
- The master coil current (~0.15 A) now runs through the board's `+12V_PROT`, well within the
  2 A input fuse and the 1.5 mm power traces.
- ch6–8 are not broken out. Adding a sixth driven load needs a board revision.

## Alternatives considered
- **8-pin coil connector (two 4-pin blocks) for all ULN outputs:** more terminals than loads.
  Dropped once the headlight came off the ULN.
- **Headlight shed relay on a ULN channel:** puts forward lighting behind the ESP. Rejected
  (fail-ON rule, CLAUDE.md hard constraint).
- **Reusing a box relay as the master:** the master must gate the box's own supply and the
  direct-out fuses, so it can't sit inside the box it gates.
- **AUX on BTN8's GPIO 32:** costs a button input. GPIO 12 with a pull-down is safe, so the
  button stays.
