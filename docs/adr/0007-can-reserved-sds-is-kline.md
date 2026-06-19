# 7. CAN reserved for own modules; the bike's data is K-line SDS, not CAN

- Status: Accepted
- Date: 2026-06-19
- Deciders: Miki

## Context
We want future expansion (remote sensor/GPS/LTE nodes) and possibly to read the bike's own
engine data. The "fairings off once" philosophy means provisioning the harness now is far
cheaper than retrofitting later — so we need to know what the bike's data bus actually is.

## Decision
The **2013 DL650 has no CAN bus** — its data lives on **K-line SDS** (Suzuki Diagnostic System,
6-pin diagnostic connector). Therefore **CAN is reserved for our own module network**, not for
talking to the bike: ESP32 built-in TWAI controller + an **SN65HVD230** transceiver, a twisted
**CAN_H/CAN_L** pair as a linear backbone with **120 Ω at both ends** and node drops carrying
12 V + GND + CAN. To read bike data we interface the **SDS/K-line separately** (an Arduino-style
SDS reader) and may later bridge SDS → our CAN. Keep the diagnostic connector accessible.

## Consequences
- No expectation of "tapping CAN" for engine data — that path is K-line and a separate effort.
- Own CAN is provisioned now at near-zero cost: a transceiver footprint, two GPIOs, a twisted
  pair with end terminations, and node drops at likely locations (front, tail).
- A future SDS-to-CAN bridge can surface engine data on the dashboard without redesign.

## Alternatives considered
- **Assume the bike exposes CAN/OBD for data**: rejected — it does not; it's K-line SDS.
- **Skip CAN provisioning entirely**: rejected — retrofitting a backbone through the fairings
  later violates "fairings off once."
