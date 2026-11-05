# 17. ULN COM and the master-relay coil on always-hot +12, not the gated box rail

- Status: Accepted
- Date: 2026-11-04
- Deciders: Miki
- Amends: [ADR-0003](0003-uln2803-coil-driver.md)

## Context
[ADR-0003](0003-uln2803-coil-driver.md) put the box's commoned coil pin 86 on +12 V and tied the
ULN2803 **COM (pin 10) to "the +12 rail (= the commoned 86)"**. The mainboard spec followed it:
net `COIL_RAIL` = J2.1 = U2.10, "externally wired to box pin 86".

The box's pin 86 is being wired to the box's own red supply, the feed that also goes to the fuses
for each relay's pin 30. That supply is **downstream of the external master relay**
([ADR-0004](0004-master-relay-gating-and-failon-headlights.md)). The master relay is also driven
by the same ULN (ch1, `MASTER_EN`). Read literally, ADR-0003 causes two faults:

1. **Master can't start.** If the master coil's +12 is taken from the box rail, it is powered by
   its own output and can never close.
2. **Master can turn itself on.** Each ULN output has a clamp diode to COM (anode = output,
   cathode = COM). Put the master coil + on always-hot and COM on the box rail: with the master
   OFF the box rail is at 0 V, so current flows from always-hot, through the master coil, through the
   ch1 clamp diode into COM, and out through the box rail into whatever load hangs there (the
   tablet buck on the 5 A direct output, for example). At ~80 Ω coil resistance that is ~0.1–0.15 A,
   which is the pull-in range of a typical automotive relay. The master can chatter or close
   with the ESP commanding it OFF, which breaks fail-OFF.

## Decision
Split the two +12 nodes:

- **Box coil common (black, pin 86 of the four box relays)** goes to the **box's red supply**
  (the master-gated feed). The box coils are only live when the box is, which matches the hardware
  gate.
- **ULN COM (pin 10)**, the board's `COIL_RAIL` net / terminal **J2.1**, and the **master relay's
  coil + (its 86)** go to **always-hot +12**, the fused battery tap on the battery side of the
  master relay. It is the same tap that feeds the ESP board (2 A inline fuse).

Each white pin-85 wire, and the master coil's 85, land on J3 as before. Firmware is unchanged.

No PCB change: `COIL_RAIL` already comes in on its own terminal (J2). Only what J2 connects to
changes, from the box's pin 86 to always-hot.

## Why this is safe
- **Box powered (master closed):** the box rail is the battery voltage minus a small drop, so
  it is no higher than always-hot. At turn-off, each box coil's 85 line kicks up and clamps
  through its diode to always-hot, about 0.7 V above its own supply. Flyback is still absorbed.
- **Box off (master open):** the box coils' 85 lines sit at ~0 V and COM sits at 12 V, so the
  diodes are reverse-biased and nothing flows.
- **Master coil:** its supply and its clamp share the same node, which is the textbook ULN
  connection.

## Consequences
- One extra conductor between the boards and the always-hot tap: J2.1 plus the master coil +.
  Both are low-current (COM carries only flyback; master coil ~0.15 A).
- The box keeps a single hidden black wire to its red supply. The box needs no other changes.
- The net name `COIL_RAIL` stays in KiCad. Its meaning is now "always-hot +12 for ULN COM", not
  "the box's pin 86".
- A future board rev could tie `COIL_RAIL` to `+12V_PROT` on-board and drop the external jumper.

## Alternatives considered
- **COM on the box rail, master coil fed from the box rail too:** the master can never close
  (fault 1).
- **COM on the box rail, master driven by a separate transistor and diode outside the ULN:**
  works, but adds parts to avoid one wire.
- **Leave COM unconnected:** no flyback clamping. The ULN outputs would avalanche on every
  turn-off. Rejected.
