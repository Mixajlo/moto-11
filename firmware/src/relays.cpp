// ===== relays.cpp =====
#include "relays.h"
#include "pins.h"

RelayController Relays;

namespace {
struct Channel { const char* name; uint8_t pin; };

// Index by RelayId. Keep this order in sync with the enum.
const Channel CH[RELAY_COUNT] = {
  { "master", PIN_MASTER_EN },
  { "fog",    PIN_FOG_EN    },
  { "grip",   PIN_GRIP_EN   },
  { "spare",  PIN_SPARE_EN  },
};
}  // namespace

void RelayController::begin() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    // Drive low first, then enable the output, so the pin never glitches HIGH
    // on the transition into OUTPUT mode.
    digitalWrite(CH[i].pin, LOW);
    pinMode(CH[i].pin, OUTPUT);
    digitalWrite(CH[i].pin, LOW);
    state_[i] = false;
  }
}

void RelayController::set(RelayId id, bool on) {
  if (id >= RELAY_COUNT) return;
  digitalWrite(CH[id].pin, on ? HIGH : LOW);
  state_[id] = on;
}

bool RelayController::isOn(RelayId id) const {
  return id < RELAY_COUNT ? state_[id] : false;
}

void RelayController::allOff() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) set(static_cast<RelayId>(i), false);
}

bool RelayController::anyOn() const {
  for (uint8_t i = 0; i < RELAY_COUNT; i++)
    if (state_[i]) return true;
  return false;
}

const char* RelayController::name(RelayId id) const {
  return id < RELAY_COUNT ? CH[id].name : "?";
}

uint8_t RelayController::pin(RelayId id) const {
  return id < RELAY_COUNT ? CH[id].pin : 0xFF;
}
