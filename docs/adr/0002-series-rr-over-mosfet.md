# 2. Series R/R over a MOSFET (shunt) regulator

- Status: Accepted
- Date: 2026-06-19
- Deciders: Miki

## Context
The DL650 ships with a **shunt-type** regulator/rectifier: the stator produces near-maximum
output continuously and the R/R dumps the excess to ground as heat, so the stator runs hot
regardless of electrical draw — a known weak point on this generation. We are adding
accessory load and want both reliable charging headroom and reduced stator stress.

## Decision
Upgrade to a **series regulator (Shindengen SH775 ≈ 35 A, or SH847 ≈ 50 A)** — not a MOSFET
unit. A MOSFET R/R (e.g. FH012AA) is still a *shunt* regulator: it runs the R/R cooler but
leaves the stator at full output. A **series** R/R disconnects the stator when charge isn't
needed, which is the actual stator-heat reduction. The SH775 is right-sized for the 650; the
**SH847 is the genuine OEM series unit (Suzuki PN 32800-31J00)**. Buy genuine — counterfeit
SH847s are shunt units in disguise.

## Consequences
- Lower stator temperature and longer life; genuine charging headroom for accessories.
- Requires hardwiring (ditch the OEM connectors), 10 AWG output on a 30 A fuse, and a sense
  lead to battery +. Healthy result ~14.2–14.4 V at the battery.
- Counterfeit risk means sourcing care (Suzuki PN, Japanese-made, metal base, Shindengen logo).
- The stator itself is **not** pre-emptively replaced — it is measured first (see ADR-0005).

## Alternatives considered
- **MOSFET R/R (FH012AA / FH020AA)**: rejected — cooler R/R but still shunt, so the stator
  stays hot.
- **Leave the stock shunt R/R**: rejected — the heat issue plus added accessory load.
