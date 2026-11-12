# 22. PWM expansion board on the I²C header; mainboard capped at four relay outputs

- Status: Accepted (designed; untested hardware, ordered together with mainboard v1)
- Date: 2026-11-13
- Deciders: Miki

## Context
- Mainboard v1 drives the master relay plus four relay outputs (OUT1–OUT4, ADR-0018), and that
  uses every clean ESP32 GPIO.
  - The ULN2803 has three spare channels, but no clean pin is left to drive them: 0/2/5/15 are
    boot-strapping pins, 1/3 are UART, 6–11 are flash, and 34–39 are input-only.
  - The terminal edge is full.
- The owner wants proportional heat for heated grips and a heated seat rather than relay on/off.
- Other builders of this open design will want different loads (factory grips, no fog, a DRL…).
- PCBs are cheap, but shipping and customs cost more than the boards. So any second board has
  to be ordered **with** mainboard v1.
- Two existing rules apply: accessories are fail-OFF, and the ESP is the single source of truth.

## Decision
- **The mainboard stays at the master relay plus at most four relay outputs.** Anything beyond
  that needs a new or additional board, not a mainboard rework.
- **First expansion board: `hardware/pwm-board/`.** It connects to the mainboard's **J6 I²C
  header** over a short cable (< about 30 cm) and has:
  - a **PCA9685 at 0x41** (0x40 is the INA3221), with ALLCALL disabled in firmware;
  - **2× TC4427A gate drivers**, supplied from the load feed and clamped to 15 V;
  - **4× AOD4184A low-side MOSFETs, up to 8 A each**, about 200 Hz PWM;
  - per channel: a mini-blade fuse, an SS34 flyback diode and an activity LED;
  - a spare 4-channel 3.3 V PWM header.
- **Load power** comes from the relay box's **master-gated** output, so key-off or ESP death
  turns the heaters off twice over.
- **Fail-OFF in every state:**
  - driver-input and gate pull-downs hold the outputs off;
  - the PCA9685 powers up full-OFF;
  - firmware clears all PCA outputs at boot (this covers "ESP reset, PCA still running");
  - only an alive-but-hung ESP holds a state, and the task watchdog ends that.
- In firmware the outputs become **OUT5–OUT8**, with the same per-output config as OUT1–OUT4
  (ADR-0021) plus a PWM duty.
- **The firmware is proven in simulation only** until the board exists; the owner has no
  PCA9685 on hand.

## Consequences
- **Easier:**
  - proportional heaters;
  - four more loads without touching the mainboard;
  - no ESP boot-pin compromises;
  - the open design scales by adding boards on the I²C bus.
- **Harder:**
  - a fine-pitch TSSOP-28 to hand-solder;
  - a second ground path (heater return on PGND), which must land at the same point as the
    mainboard GND so heater current never shares the thin I²C ground;
  - an I²C bus that must stay short.
- **Follow-on:**
  - the `pca9685` driver with host tests;
  - OUT5–OUT8 in the ADR-0021 output config;
  - bring-up on the real board (bench relay guide + PWM walk).
- **Board v1 facts:**
  - 90 × 70 mm, 2 layers, 1 oz copper;
  - power on pours: a +12 V strip on B.Cu, per-channel OUT± pours on F.Cu, GND on both layers;
  - DRC 0 unconnected, 0 parity issues. Accepted items: two single-spoke GND thermals on U1, and
    0.7 mm connector pin-name silk.

## Alternatives considered
- **GPIO 5/15 as PWM outputs:**
  - they are boot-strapping pins that toggle at reset;
  - they are not broken out on v1;
  - they give only 2 channels.
  
  Kept only as a fallback.
- **PCF8574/MCP23017 expander:** on/off only, no hardware PWM. Still the choice for extra on/off
  relays.
- **Bigger mainboard rev:** it would delay the order. The I²C header was already there.
- **Through-hole IRFZ44N / IRF540 from an assortment kit:** they would run warmer (17–44 mΩ),
  and kit parts are often counterfeit. The AOD4184A (about 7 mΩ, DPAK on copper) ships in the
  same LCSC parcel.
- **1N4148 as flyback:** far too small for the load current at each turn-off.
