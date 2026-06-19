# Architecture Decision Records

Short records of the significant decisions on **moto-11** — the *why* behind the build, so
future choices (and Claude Code) start with the full context instead of re-deriving it.

One file per decision, named `NNNN-short-title.md`, following Michael Nygard's template
(see `template.md`). A decision's `Status` moves Proposed → Accepted; when a later decision
overturns an earlier one we add a new ADR and mark the old one *Superseded* rather than
editing history.

## Index
- [0001](0001-esp32-authoritative-controller.md) — ESP32 as the authoritative controller; tablet as a thin BLE dashboard
- [0002](0002-series-rr-over-mosfet.md) — Series R/R over a MOSFET (shunt) regulator
- [0003](0003-uln2803-coil-driver.md) — ULN2803 coil driver; commoned pin 86 reused as +12, low-side on pin 85
- [0004](0004-master-relay-gating-and-failon-headlights.md) — External master relay gates the box; headlights stay outside it (fail-ON)
- [0005](0005-ina226-sensing.md) — INA226 for engine-run detection and battery telemetry; sense on a clean lead
- [0006](0006-layered-connectivity.md) — Layered connectivity: phone-relay BLE when parked, tablet radio when riding
- [0007](0007-can-reserved-sds-is-kline.md) — CAN reserved for own modules; the bike's data is K-line SDS, not CAN
- [0008](0008-ota-ab-rollback.md) — OTA with A/B rollback; bench-twin-first
