// ===== buttons.cpp =====
#include "buttons.h"
#include "pins.h"
#include "log.h"

Buttons Btns;

namespace {
constexpr uint32_t DEBOUNCE_MS = 25;

// Default button -> relay map. RELAY_COUNT means "unmapped" (logged, no action).
// MASTER is deliberately not mapped — the supervisor owns it; a button must not
// fight the engine-run gating. Mapping becomes runtime config later (ADR-0013).
const RelayId MAP[BUTTON_COUNT] = {
  RELAY_FOG,    // BTN1
  RELAY_GRIP,   // BTN2
  RELAY_SPARE,  // BTN3
  RELAY_COUNT,  // BTN4 (spare)
  RELAY_COUNT,  // BTN5
  RELAY_COUNT,  // BTN6
  RELAY_COUNT,  // BTN7
  RELAY_COUNT,  // BTN8 (spare GPIO)
};
}  // namespace

void Buttons::begin() {
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(PIN_BTN[i], INPUT_PULLUP);
    stable_[i]     = false;       // not pressed
    lastRead_[i]   = false;
    lastChange_[i] = millis();
  }
}

RelayId Buttons::mapping(uint8_t i) const { return i < BUTTON_COUNT ? MAP[i] : RELAY_COUNT; }
bool    Buttons::pressed(uint8_t i) const { return i < BUTTON_COUNT ? stable_[i] : false; }

void Buttons::fire(uint8_t i) {
  if (i >= BUTTON_COUNT) return;
  RelayId r = MAP[i];
  if (r == RELAY_COUNT) { LOGI("BTN%u pressed (unmapped)", i + 1); return; }
  bool on = !Relays.isOn(r);        // momentary: a press toggles the channel
  Relays.set(r, on);
  LOGI("BTN%u -> %s %s", i + 1, Relays.name(r), on ? "ON" : "off");
}

void Buttons::update() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    bool raw = (digitalRead(PIN_BTN[i]) == LOW);   // active-low: pressed pulls to GND
    if (raw != lastRead_[i]) {
      lastRead_[i]   = raw;
      lastChange_[i] = now;                        // movement: restart the debounce window
    } else if (raw != stable_[i] && now - lastChange_[i] >= DEBOUNCE_MS) {
      stable_[i] = raw;                            // settled into a new state
      if (raw) fire(i);                            // act on the press edge only
    }
  }
}
