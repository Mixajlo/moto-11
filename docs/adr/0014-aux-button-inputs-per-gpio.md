# 14. Aux button inputs: one-wire-per-button, read per-GPIO, on a JST-XH connector

- Status: Accepted
- Date: 2026-06-23
- Deciders: Miki

## Context
A motorcycle handlebar switch case (7 buttons — 5 front, 2 back) will drive aux
loads (fog, grips, spare, …) from the bars. The buttons are **dry contacts** (simple
switch closures, no power). The owner wants **one wire per button** run from the bars
to the ESP, a connector on the mainboard to plug the cable into, and headroom to add
a few more buttons later. (Whether each is momentary or maintained is to be confirmed
when the case is in hand — it changes firmware interpretation, not the wiring.)

## Decision
- **Wiring:** one conductor per button from the case to the mainboard, sharing a
  **common ground**, landing on a keyed **JST-XH** connector (serviceable unplug).
  Provision **8 button inputs + GND** (7 used now, 1 spare).
- **Read per-GPIO:** assign 8 free, pull-up-capable GPIOs —
  **`4, 14, 16, 17, 18, 19, 23, 32`** (`BTN1..BTN8`). **Active-low**: enable the
  internal pull-up, the button closes the line to GND (pressed = LOW). Add a small
  **RC filter** per line + **firmware debounce**. No opto-isolation — they're dry
  contacts referenced to our ground.
- **Buttons are inputs that drive the relay outputs** (master/fog/grip/spare + the
  spare ULN channels). The **button → action mapping is runtime config**
  ([ADR-0013](0013-runtime-config-nvs-console.md)) — default mapping TBD. Firmware
  supports both **momentary** (press = toggle / step) and **maintained** (position =
  state) switches; the exact handling is set once the switch type is validated.
- **Upgrade path:** the per-GPIO ceiling is ~8–10 clean pins. Because the harness is
  one-conductor-per-button into a connector, outgrowing it is a **board-rev swap to an
  MCP23017 I²C expander** (16 inputs + INT on the existing I²C bus) — route the same
  connector pins to the expander instead of the GPIOs, **no change to the bike harness**.

## Consequences
- Simplest firmware and lowest latency; works on the bench immediately (a jumper to
  GND on a BTN pin = a press).
- Consumes most of the remaining ESP GPIOs — accepted; future peripherals that need
  pins should go on I²C (and a button move to the MCP23017 frees the GPIOs back).
- RTC-capable pins among the set (4, 14, 32) keep a future **wake-on-button** option
  open without extra cost.
- Adds an 8-way JST-XH + 8 RC networks to the board (provision now, since the PCB
  isn't fabbed). The `pins.h` map and the mainboard schematic spec gain the BTN lines.

## Alternatives considered
- **MCP23017 I²C expander now:** more scalable (16 in, 1 INT, no GPIO cost) but more
  board complexity than needed for 7–8 buttons; kept as the documented upgrade path.
- **Resistor ladder on one ADC pin (fewest wires):** rejected — ESP32 ADC is noisy/
  non-linear, only one button at a time is unambiguous; poor fit for handlebar use.
- **Remote button module over CAN/serial:** v2 overkill for a short handlebar run.
- **Switched-12 V or illuminated buttons:** would need LED power + opto-isolation;
  not these buttons (dry contacts), but the connector can be widened later if needed.
