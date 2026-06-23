// ===== supervisor.h — autonomous power/engine-run state machine =====
// States: SLEEP -> ARMED -> RUNNING <-> POWERED, plus OFF_DELAY on key-off.
//
//   SLEEP      key off; (real hw) deep-sleep, wake on ignition.
//   ARMED      key on, engine not yet confirmed; MASTER off (a battery tender
//              can't power the box because that needs the key on AND charging).
//   RUNNING    key on AND engine running (Vbus >= V_RUN_ON); MASTER on.
//   POWERED    key on, engine OFF (stall / kill switch / brief stop); MASTER
//              STAYS on so accessories survive a restart — but engine-only loads
//              (e.g. heated grips) may be shed since we're not charging.
//   OFF_DELAY  key off; MASTER held briefly, then cut + sleep.
//
// The ignition key is the master switch: once powered, accessories stay on until
// the key goes OFF. A long battery-save backstop cuts power if the engine is left
// off with the key on (forgotten key).
#pragma once

#include <Arduino.h>

enum SupState : uint8_t { SUP_SLEEP, SUP_ARMED, SUP_RUNNING, SUP_POWERED, SUP_OFF_DELAY };

class Supervisor {
public:
  void begin();
  void update();                       // call every loop()

  SupState    state() const { return state_; }
  const char* stateName() const;
  bool engineRunning() const { return state_ == SUP_RUNNING; }  // charging — engine-only loads OK

  // Remaining ms until the next time-based transition (POWERED backstop or
  // OFF_DELAY). Returns 0 if the current state has no countdown. If `what` is
  // non-null it receives a short label of the pending transition (nullptr when
  // there is no active timer).
  uint32_t timeLeftMs(const char** what = nullptr) const;

  // Tunables (future: tablet-configurable over BLE — see roadmap).
  float    vRunOn          = 13.2f;    // V: engine confirmed running
  float    vRunOff         = 12.9f;    // V: below = not charging (engine stopped)
  uint32_t runDebounceMs   = 1000;     // ms above vRunOn before RUNNING
  uint32_t stallDebounceMs = 2000;     // ms below vRunOff before RUNNING -> POWERED
  uint32_t offDelayMs      = 30000;    // ms MASTER held after KEY-OFF before cut+sleep
  uint32_t backstopMs      = 600000;   // ms in POWERED before battery-save cut (10 min; 0 = off)
  // Charge-health (while RUNNING). Hysteresis deadband + debounce so bus ripple and
  // brief load-switch dips near the threshold don't raise/clear a false warning:
  //   marginal  when vbus <  chargeWarn  sustained chargeDebounceMs
  //   cleared   when vbus >= chargeClear sustained chargeDebounceMs
  // Clean ladder: vRunOff 12.9 < vRunOn 13.2 < chargeWarn 13.4 < chargeClear 13.6.
  // (The INA3221 driver will also hardware-average samples to pre-filter noise.)
  float    chargeWarn       = 13.4f;   // V: below = heading marginal
  float    chargeClear      = 13.6f;   // V: at/above = healthy again (deadband 13.4..13.6)
  uint32_t chargeDebounceMs = 5000;    // ms a charge condition must hold before the flag flips

  bool chargeMarginal() const { return marginal_; }   // latched flag for UI/load-shed later

private:
  void enter(SupState s);
  void checkChargeHealth(float vbus);

  SupState state_       = SUP_SLEEP;
  uint32_t aboveSince_  = 0;           // run-debounce timer  (0 = not counting)
  uint32_t belowSince_  = 0;           // stall-debounce timer (0 = not counting)
  uint32_t offTimer_    = 0;           // off-delay start
  uint32_t poweredSince_= 0;           // POWERED entry time (for the backstop)
  uint32_t lastTick_    = 0;           // throttle for the periodic debug log
  uint32_t warnTick_    = 0;           // throttle for the charge-marginal nag
  uint32_t chgSince_    = 0;           // charge-health debounce timer (0 = not counting)
  bool     marginal_    = false;       // current charge-marginal state (debounced)
};

extern Supervisor Engine;
