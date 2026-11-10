// ===== loads.h — accessory load policy: engine-only gating + load-shedding (ADR-0019) =====
// The rider says what they WANT (buttons -> request()); this decides what is ALLOWED and
// drives the relays to (wanted AND allowed). Managed loads, in shed order:
//
//   GRIP  engine-only: held off unless the engine is RUNNING (no charging = no grips).
//         Shed first when charging is marginal.
//   FOG   shed second, only if charging is still marginal one shedStep after the grips.
//
// Both are also held off while cranking (START held, plus crankHold after release), so
// the starter gets the battery. Headlights are NOT here: they are off the ESP by design
// (ADR-0004/0018). MASTER stays supervisor-owned. SPARE/AUX pass straight through.
//
// A shed load is remembered as wanted and comes back by itself: one restore step per
// shedStep once charging has recovered (reverse order: fog, then grips), and grips come
// on when the engine (re)starts. A request for a blocked load is remembered the same way.
//
// The relays only move when the computed state changes, so the bench console's raw
// `on/off` is a bypass until the next change. If a new block reason appears while a
// managed relay is physically on (e.g. switched on from the console), it is shed and
// treated as wanted, so it is restored with the rest.
//
// Inputs come in as a plain struct (main.cpp fills it from Engine/Sensor) so this has
// no dependency on the supervisor and is unit-testable on the host.
#pragma once

#include <Arduino.h>
#include "relays.h"

struct LoadInputs {
  bool engineRunning  = false;   // supervisor RUNNING (charging)
  bool chargeMarginal = false;   // supervisor's debounced marginal-charging flag
  bool cranking       = false;   // START held (raw; crankHold is applied here)
};

enum ShedWhy : uint8_t {         // bit flags: why a managed load is held off
  WHY_NONE       = 0,
  WHY_ENGINE_OFF = 1,            // grips only: engine not running
  WHY_CHARGE     = 2,            // shed for marginal charging
  WHY_CRANK      = 4,            // starter cranking
};

class LoadManager {
public:
  // Tunables (persisted by Config, ADR-0013).
  uint32_t shedStepMs  = 30000;  // dwell between shed steps (and between restore steps)
  uint32_t crankHoldMs = 1500;   // hold loads off this long after START release; 0 = no crank shed

  void begin();
  void update(const LoadInputs& in);   // call every loop, after the supervisor

  void request(RelayId r, bool on);    // rider intent; unmanaged relays switch directly
  bool managed(RelayId r) const { return r == RELAY_GRIP || r == RELAY_FOG; }
  bool wanted(RelayId r) const;        // managed: rider intent; unmanaged: relay state
  uint8_t blockedWhy(RelayId r) const; // ShedWhy flags, WHY_NONE = allowed
  uint8_t shedLevel() const { return level_; }   // 0 none, 1 grips, 2 grips + fog
  bool crankHold() const;              // cranking, or inside crankHold after release

private:
  void apply();
  void stepLadder(uint32_t now);

  LoadInputs in_;
  bool     want_[RELAY_COUNT]    = { false };
  bool     applied_[RELAY_COUNT] = { false };   // last state this manager drove
  uint8_t  lastWhy_[RELAY_COUNT] = { 0 };
  uint8_t  level_     = 0;
  uint32_t stepAt_    = 0;         // time of the last shed/restore step (or recovery)
  bool     wasMarginal_ = false;
  bool     wasCranking_ = false;
  uint32_t crankRelease_ = 0;
  bool     crankEver_   = false;   // a release time is valid
};

extern LoadManager Loads;
