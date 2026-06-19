<!-- SPDX-License-Identifier: CERN-OHL-P-2.0 -->
# Mainboard — design rules, stackup & mechanical

Constraints for the KiCad PCB. These are chosen for a **low-current control board** in an
automotive (vibration + thermal + transient) environment, fabbed cheaply at JLCPCB/PCBWay.

## Currents on this board (why the traces are modest)

This board does **not** carry load current. The worst-case currents it sees:

| Path | Current | Notes |
|------|---------|-------|
| `+12V_HOT` → buck | < 0.5 A | ESP + peripherals via the 5 V buck. |
| `COIL_RAIL` / coil outputs | ~0.15–0.2 A each, ≤ ~1 A total | Relay coils; brief inrush. |
| `+5V` | < 0.6 A | ESP peak (WiFi TX bursts). |
| `+3V3` | < 0.3 A | Sensors + opto pull-ups. |
| Sense / signal | µA–mA | INA226 lead, opto, I²C, IMU. |

So the dominant sizing factor is **robustness**, not heat: wide power traces and a solid ground
pour resist vibration fatigue and give clean returns, even though the amps are small.

## Trace widths (1 oz / 35 µm copper, ≤ 10 °C rise)

| Class | Nets | Width | Rated (10 °C rise) |
|-------|------|-------|--------------------|
| Power | `+12V_HOT`, `+12V_PROT`, `+5V`, `COIL_RAIL`, coil outputs | **1.5 mm** (≈ 59 mil) | ~3 A — generous margin |
| 3V3 rail | `+3V3` | **0.8 mm** | ~1.7 A |
| Signal | GPIO, I²C, opto, INT | **0.30 mm** (≈ 12 mil) | ample |
| Ground | copper **pour** both layers, stitched | — | star-tied off-board |

> Going wider than strictly needed on power is deliberate — it costs nothing and buys vibration
> and transient headroom on a bike.

## Clearances & rules (JLCPCB 2-layer default-safe)

- Min track / clearance: **0.2 mm** (8 mil) — well within fab capability; use 0.25 mm where space allows.
- **12 V-to-anything clearance: ≥ 0.5 mm**; **opto isolation barrier: keep ≥ 4 mm** creepage
  between the 12 V (input) side and the logic (output) side of U4/U5 — do not pour ground under
  the barrier.
- Min via: 0.3 mm drill / 0.6 mm pad. Vias: 0.4/0.8 for power.
- Hole-to-copper: 0.25 mm. Silk min: 0.15 mm / 1 mm text height.

## Stackup

- **2-layer**, 1.6 mm FR-4, 1 oz copper, HASL (lead-free) finish.
- Top: components + signal; Bottom: ground pour + power fills + a few signal jumpers.
- Ground pour on **both** layers, stitched with vias every ~10 mm and around the ESP/RF area.

## Board outline & mechanical

- Target **~90 × 70 mm** (fits DevKitC ~55 × 28 mm + ULN + module headers + terminal blocks on
  one edge). Adjust after placement.
- **4× M3 mounting holes**, 4 mm from corners, with keep-out and a ground stitch ring (chassis
  option via a 0 Ω / ferrite, default isolated — star ground only).
- **Terminal blocks on one board edge** (J1–J5) so field wiring enters from one side; pluggable
  type preferred for serviceability.
- Keep the **DevKitC antenna end overhanging the board edge** (no copper under the PCB-antenna
  keep-out) — critical for WiFi/BLE range.
- USB port of the DevKitC must remain **physically accessible** for reflashing (orient the
  socket so USB faces a board edge; see electrical-plan §05 service-loop note).

## RF / analog hygiene

- ESP32 antenna keep-out: no copper (any layer) under/around the WROOM antenna cutout.
- Route I²C as a pair, away from the buck switch node; keep INA226 sense (`VBUS_SENSE`/`R14`/`C10`)
  short and away from the buck.
- Place buck input/output bulk caps tight to A3; keep the buck's high-di/dt loop small.
- One 100 nF per IC power pin, within ~5 mm.

## Fabrication

- House rules: **JLCPCB** (cheap, 2-layer). Generate Gerbers + drill + a centroid/BOM if you
  later want assembly; for hand-assembly, Gerbers + drill are enough.
- Panelization: none needed (single board).
- Export step is in [04-kicad-guide.md](04-kicad-guide.md) §7.
