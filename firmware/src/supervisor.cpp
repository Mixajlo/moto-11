// ===== supervisor.cpp =====
#include "supervisor.h"
#include "sensors.h"
#include "relays.h"
#include "log.h"

Supervisor Engine;

namespace {
const char* const NAMES[] = { "SLEEP", "ARMED", "RUNNING", "POWERED", "OFF_DELAY" };
}

const char* Supervisor::stateName() const { return NAMES[state_]; }

void Supervisor::begin() {
  state_ = SUP_SLEEP;
  LOGI("supervisor: start in SLEEP");
}

void Supervisor::enter(SupState s) {
  LOGI("supervisor: %s -> %s", NAMES[state_], NAMES[s]);
  state_ = s;
  aboveSince_ = 0;
  belowSince_ = 0;
  if (s != SUP_RUNNING) marginal_ = false;     // charge-health only meaningful while running
  if (s == SUP_POWERED) poweredSince_ = millis();
}

// While RUNNING: flag/throttle-warn when the bus is charging but marginal.
void Supervisor::checkChargeHealth(float vbus) {
  bool m = (vbus < chargeWarnHi && vbus >= chargeWarnLo);
  if (m && !marginal_) LOGW("charging MARGINAL: vbus=%.2f — loads too high or R/R weak", vbus);
  if (!m && marginal_) LOGI("charging recovered: vbus=%.2f", vbus);
  marginal_ = m;
  if (m && millis() - warnTick_ >= 15000) {     // keep nagging, but not every loop
    warnTick_ = millis();
    LOGW("charging still marginal: vbus=%.2f", vbus);
  }
}

void Supervisor::update() {
  const bool  ign  = Sensor.ignition();
  const float vbus = Sensor.busVoltage();

  switch (state_) {

    case SUP_SLEEP:
      // Real hardware deep-sleeps and wakes on the ignition edge; on the bench
      // (SIM) we must not sleep (it would drop WiFi/console), so we just idle.
      if (ign) {
        enter(SUP_ARMED);
      } else if (!Sensor.sim()) {
        LOGI("supervisor: would deep-sleep here (wake on ignition)");
        // TODO(hardware): esp_sleep_enable_ext0_wakeup(IGN_SENSE, 0);
        //                 esp_deep_sleep_start();
      }
      break;

    case SUP_ARMED:                          // key on, engine not yet confirmed
      if (!ign) { enter(SUP_SLEEP); break; }
      if (vbus >= vRunOn) {
        if (aboveSince_ == 0) {
          aboveSince_ = millis();
          LOGD("armed: vbus %.2f >= %.2f, debouncing", vbus, vRunOn);
        } else if (millis() - aboveSince_ >= runDebounceMs) {
          Relays.set(RELAY_MASTER, true);
          LOGI("supervisor: engine confirmed (vbus=%.2f) — MASTER on", vbus);
          enter(SUP_RUNNING);
        }
      } else {
        aboveSince_ = 0;
      }
      break;

    case SUP_RUNNING:                        // key on, engine running, MASTER on
      if (!ign) {
        offTimer_ = millis();
        LOGI("supervisor: key off — holding %lus before cut",
             static_cast<unsigned long>(offDelayMs / 1000));
        enter(SUP_OFF_DELAY);
        break;
      }
      if (vbus < vRunOff) {                  // engine may have stopped (key still on)
        if (belowSince_ == 0) {
          belowSince_ = millis();
          LOGD("running: vbus %.2f < %.2f, watching for stall", vbus, vRunOff);
        } else if (millis() - belowSince_ >= stallDebounceMs) {
          LOGI("supervisor: engine stopped, key on (vbus=%.2f) — POWERED (power held)", vbus);
          enter(SUP_POWERED);                // MASTER STAYS on; engine-only loads shed
        }
      } else {
        belowSince_ = 0;
        checkChargeHealth(vbus);             // marginal-charging warning lives here
        if (millis() - lastTick_ >= 2000) {
          lastTick_ = millis();
          LOGD("running: vbus=%.2f", vbus);
        }
      }
      break;

    case SUP_POWERED:                        // key on, engine OFF, MASTER still on
      if (!ign) {
        offTimer_ = millis();
        LOGI("supervisor: key off — holding %lus before cut",
             static_cast<unsigned long>(offDelayMs / 1000));
        enter(SUP_OFF_DELAY);
        break;
      }
      if (vbus >= vRunOn) {                  // engine restarted
        if (aboveSince_ == 0) {
          aboveSince_ = millis();
          LOGD("powered: vbus %.2f >= %.2f, debouncing", vbus, vRunOn);
        } else if (millis() - aboveSince_ >= runDebounceMs) {
          LOGI("supervisor: engine running again (vbus=%.2f) — RUNNING", vbus);
          enter(SUP_RUNNING);
          break;
        }
      } else {
        aboveSince_ = 0;
      }
      // Battery-save backstop: engine left off with the key on for too long.
      if (backstopMs && millis() - poweredSince_ >= backstopMs) {
        Relays.set(RELAY_MASTER, false);
        LOGW("supervisor: engine off + key on for %lumin — MASTER off to save battery",
             static_cast<unsigned long>(backstopMs / 60000));
        enter(SUP_ARMED);                    // master off; re-powers if the engine restarts
        break;
      }
      if (millis() - lastTick_ >= 5000) {
        lastTick_ = millis();
        LOGD("powered: vbus=%.2f (engine off, power held)", vbus);
      }
      break;

    case SUP_OFF_DELAY:                      // key off, MASTER held, counting down
      if (ign) {                             // key back on within the window — keep power
        if (vbus >= vRunOn) { LOGI("supervisor: engine running again — RUNNING"); enter(SUP_RUNNING); }
        else                { LOGI("supervisor: key back on — POWERED"); enter(SUP_POWERED); }
        break;
      }
      if (millis() - offTimer_ >= offDelayMs) {
        Relays.set(RELAY_MASTER, false);
        LOGI("supervisor: off-delay expired — MASTER off");
        enter(SUP_SLEEP);
      }
      break;
  }
}
