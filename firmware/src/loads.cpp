// ===== loads.cpp =====
#include "loads.h"
#include "log.h"

LoadManager Loads;

namespace {
// Shed order (ADR-0004/0018: grips -> fog only; the ESP never sheds forward light).
constexpr RelayId ORDER[] = { RELAY_GRIP, RELAY_FOG };
constexpr uint8_t LEVELS  = sizeof(ORDER) / sizeof(ORDER[0]);

uint8_t orderIndex(RelayId r) {
  for (uint8_t i = 0; i < LEVELS; i++) if (ORDER[i] == r) return i;
  return LEVELS;
}

const char* whyText(uint8_t why) {
  if (why & WHY_CRANK)      return "cranking";
  if (why & WHY_ENGINE_OFF) return "engine not running";
  if (why & WHY_CHARGE)     return "charging marginal";
  return "";
}
}  // namespace

void LoadManager::begin() {
  in_ = LoadInputs{};
  for (uint8_t i = 0; i < RELAY_COUNT; i++) { want_[i] = false; applied_[i] = false; lastWhy_[i] = 0; }
  level_ = 0;
  stepAt_ = 0;
  wasMarginal_ = false;
  wasCranking_ = false;
  crankEver_ = false;
}

bool LoadManager::wanted(RelayId r) const {
  if (r >= RELAY_COUNT) return false;
  return managed(r) ? want_[r] : Relays.isOn(r);
}

bool LoadManager::crankHold() const {
  if (crankHoldMs == 0) return false;
  if (in_.cranking) return true;
  return crankEver_ && millis() - crankRelease_ < crankHoldMs;
}

uint8_t LoadManager::blockedWhy(RelayId r) const {
  if (!managed(r)) return WHY_NONE;
  uint8_t why = WHY_NONE;
  if (r == RELAY_GRIP && !in_.engineRunning) why |= WHY_ENGINE_OFF;
  if (orderIndex(r) < level_)                why |= WHY_CHARGE;
  if (crankHold())                           why |= WHY_CRANK;
  return why;
}

void LoadManager::request(RelayId r, bool on) {
  if (r >= RELAY_COUNT || r == RELAY_MASTER) return;   // MASTER is the supervisor's
  if (!managed(r)) { Relays.set(r, on); return; }
  want_[r] = on;
  const uint8_t why = blockedWhy(r);
  const bool desired = on && !why;
  Relays.set(r, desired);                // a request always lands, even after a console bypass
  applied_[r] = desired;
  lastWhy_[r] = why;
  if (on && why) LOGI("loads: %s requested - held off (%s), comes on when allowed",
                      Relays.name(r), whyText(why));
}

// One shed step: shed the next load in ORDER that is actually on. Loads that are off
// are skipped (shedding them gains nothing), so the first real shed happens now.
void LoadManager::stepLadder(uint32_t now) {
  if (!in_.engineRunning) {              // charge health is only judged while running
    if (level_) LOGI("loads: engine not running - shed ladder cleared");
    level_ = 0;
    wasMarginal_ = false;
    return;
  }
  const bool m = in_.chargeMarginal;
  const bool shedDue    = m && level_ < LEVELS && (!wasMarginal_ || now - stepAt_ >= shedStepMs);
  const bool restoreDue = !m && wasMarginal_ == false && level_ > 0 && now - stepAt_ >= shedStepMs;

  if (shedDue) {
    while (level_ < LEVELS) {
      RelayId r = ORDER[level_++];
      if (Relays.isOn(r)) { LOGW("loads: charging marginal - shedding %s (level %u)",
                                 Relays.name(r), (unsigned)level_); break; }
    }
    stepAt_ = now;
  } else if (!m && wasMarginal_) {
    stepAt_ = now;                       // recovered: wait one step before restoring
  } else if (restoreDue) {
    while (level_ > 0) {
      RelayId r = ORDER[--level_];
      if (want_[r]) { LOGI("loads: charging healthy - restoring %s (level %u)",
                           Relays.name(r), (unsigned)level_); break; }
    }
    stepAt_ = now;
  }
  wasMarginal_ = m;
}

void LoadManager::apply() {
  for (RelayId r : ORDER) {
    const uint8_t why = blockedWhy(r);
    const bool newWhy = (why & ~lastWhy_[r]) != 0;
    // A new block reason while the relay is physically on (maybe switched on from the
    // console, bypassing request()): shed it and remember it as wanted.
    const bool forceOff = newWhy && Relays.isOn(r);
    if (forceOff) want_[r] = true;
    const bool desired = want_[r] && !why;
    if (desired != applied_[r] || forceOff) {
      Relays.set(r, desired);
      if (desired)  LOGI("loads: %s ON%s", Relays.name(r), lastWhy_[r] ? " (restored)" : "");
      else if (why) LOGI("loads: %s held off (%s)", Relays.name(r), whyText(why));
      else          LOGI("loads: %s off", Relays.name(r));
    }
    applied_[r] = desired;
    lastWhy_[r] = why;
  }
}

void LoadManager::update(const LoadInputs& in) {
  const uint32_t now = millis();
  if (wasCranking_ && !in.cranking) { crankRelease_ = now; crankEver_ = true; }
  wasCranking_ = in.cranking;
  in_ = in;
  stepLadder(now);
  apply();
}
