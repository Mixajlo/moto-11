# 16. Handlebar case read through a ring scan; button mapping is runtime config

- Status: Accepted
- Date: 2026-11-01
- Deciders: Miki

## Context
[ADR-0014](0014-aux-button-inputs-per-gpio.md) assumed one signal wire per button and left the
momentary/maintained question and the mapping open. The case is now in hand: 6 momentary buttons
and 1 latching button (front red), with a 7-wire cable.

As supplied, only five buttons sit on the red common. Four buttons form a closed factory loop
(red, front white, white wire, front yellow, black wire, rear red, green wire, rear yellow, back to
red), so front yellow and rear red never reach the common by themselves and cannot be read as plain
inputs. The owner is not rewiring the case for now, and wants a different switch set later to be a
configuration change, not a firmware change.

## Decision
- **Read the case as supplied, with a ring scan.** Red goes to `GND`. The ESP reads white and green
  with black released, then pulls black low and reads them again, which reveals front yellow and
  rear red. Black is only ever pulled low, never driven high.
- **Inputs stay per-GPIO**, active-low, on the existing `J10`/`J11`, with no board change.

| Button | Cable wire | Board pin | Reported as |
|---|---|---|---|
| Front red (latching) | Blue | `J10` pin 1 | `BTN1` |
| Front green | Brown | `J10` pin 2 | `BTN2` |
| Front blue | Yellow | `J10` pin 3 | `BTN3` |
| Front white | White | `J10` pin 4 | `BTN4` |
| Front yellow | Black | `J11` pin 1 | `BTN5` |
| Rear red | Green | `J11` pin 2 | `BTN6` |
| Rear yellow | also on green | | `BTN7` |
| Common | Red | `J10` pin 5 | `GND` |

- **Ring mode is runtime config**, on by default and saved in NVS: `btn ring <on|off>`. A runtime
  setting was chosen over a macro so the later cable swap needs no reflash.
- **If the cable is later replaced with an 8-core one,** each button gets its own wire on the same
  `BTN` number, with the common on `GND`, and ring mode is turned off. Not scheduled.
- **The mapping is runtime config**, persisted in NVS under the `motobtn` namespace, as in
  [ADR-0013](0013-runtime-config-nvs-console.md). Each input stores a relay (or none) and a type:
  *momentary* (a press toggles the relay) or *latching* (the switch position sets the relay).
  Console: `btn`, `btn map <1-8> <fog|grip|spare|none> [push|latch]`, `btn reset`.
- **MASTER can never be mapped**, stays supervisor-owned, and an invalid stored value falls back to
  the firmware default.

## Consequences
- All 7 buttons work on the existing cable, and the mapping carries over unchanged to one wire per
  button.
- Up to 2 loop buttons held together read exactly. With 3 or more closed the last is shorted out and
  cannot be seen, so the loop's state is held until one is released. The three separate buttons are
  unaffected.
- The scan is slower than a plain input, because the button filters recharge through the pull-ups:
  about 28 ms per cycle plus debounce. To be confirmed on the bench, including the 25 ms settle time.
- Ring mode must be off when one wire per button is fitted, or `BTN5` is pulled low periodically and
  `BTN4-7` are misread. `BTN7`'s own input is unused while it is on.
- Any dry-contact switch set wired one wire per input, up to 8, is a console change. Other wiring
  still needs rewiring or a firmware change.
- A chafed signal wire reads as a held button, the same as a stuck switch.
- Actions are limited to relays for now. Functions such as tablet commands would extend the same
  mapping.

## Alternatives considered
- **Plain inputs only, 5 of 7 buttons:** simplest, but leaves two buttons dead until the cable is
  replaced.
- **A compile-time macro for ring mode:** would need a reflash to change, and leaves one of the two
  code paths unbuilt.
- **Rewire within the existing 7-core cable, with the common on the frame:** keeps the cable, but
  starter current can shift frame ground against the board's during cranking, and the handlebar
  clamp is not a dependable ground path. An 8-core cable avoids both.
