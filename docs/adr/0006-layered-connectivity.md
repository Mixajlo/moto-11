# 6. Layered connectivity: phone-relay BLE when parked, tablet radio when riding

- Status: Accepted
- Date: 2026-06-19
- Deciders: Miki

## Context
The bike lives in a **shared underground garage** — no WiFi, no reliable cellular, no fixed
position to phone home from — yet we want theft and battery telemetry while it's parked. The
owner and his wife pass the bike regularly on the way to the car.

## Decision
**Layer the connectivity** to the situation. **Riding:** the tablet's radio carries data
(including crash SMS). **Parked:** an **opportunistic BLE relay** — the bike queues events in
NVS and drains the queue through the owner's/wife's phones whenever they pass in range; no
bike-side radio needed in v1. **v2:** a dedicated LTE Cat-M / NB-IoT module + GPS + backup LiPo
for drive-away theft. The BLE GATT contract exposes Status / CrashFlag / EventRead / EventAck /
Config characteristics with a read → forward → ack drain, over an LE-bonded whitelist.

## Consequences
- No bike-side radio or data plan required for v1 — the phones are the radio.
- Works within the garage's hard constraints (no WiFi, no public position).
- Events persist in NVS until a phone relays them; nothing is lost between passes.
- The v2 LTE/GPS path is reserved without redesign.
- Parked liveness depends on a paired phone passing within BLE range.

## Alternatives considered
- **Bike-side WiFi in v1**: rejected — there is no garage WiFi to join.
- **Always-on LTE in v1**: deferred to v2 — cost, power draw, and complexity not justified yet.
