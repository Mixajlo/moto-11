// ===== button_ring.h — decode the handlebar switch's four-button ring (ADR-0016) =====
// Four buttons in the case are factory-wired in a closed loop, not to a common:
//
//   RED(GND) -[white]- WHITE -[front yellow]- BLACK -[rear red]- GREEN -[rear yellow]- RED
//
// So two samples are taken, both as "line is LOW":
//   idle: BLACK released (pull-up). WHITE/GREEN low = white/rear yellow closed.
//   scan: BLACK driven low. WHITE/GREEN now also go low through front yellow/rear red.
// Pure logic, no Arduino dependency, so it is unit-tested natively.
#pragma once

struct RingSample {
  bool idleWhite, idleBlack, idleGreen;   // true = line LOW with BLACK released
  bool scanWhite, scanGreen;              // true = line LOW with BLACK driven low
};

struct RingPressed {
  bool white, frontYellow, rearRed, rearYellow;
};

// With BLACK low at idle it has a path to ground, which identifies the closed pair.
// All three lines low means 3+ buttons are closed: the remaining button is shorted
// out and cannot be seen, so the previous state is held.
inline RingPressed decodeRing(const RingSample& s, const RingPressed& prev) {
  if (s.idleBlack) {
    if (s.idleWhite && s.idleGreen) return prev;
    if (s.idleWhite) return { true,  true,  false, false };
    if (s.idleGreen) return { false, false, true,  true  };
    return { false, false, false, false };   // BLACK grounded by a fault, no button explains it
  }
  RingPressed r;
  r.white       = s.idleWhite;
  r.frontYellow = !s.idleWhite && s.scanWhite;
  r.rearYellow  = s.idleGreen;
  r.rearRed     = !s.idleGreen && s.scanGreen;
  return r;
}
