// ===== buttons.h — handlebar aux buttons (ADR-0014) =====
// Dry-contact buttons read per-GPIO, active-LOW with internal pull-ups. Each
// button is debounced; a press edge fires the mapped action (toggle a relay).
// `fire()` is also callable from the console (`btn press <n>`) so the mapping is
// bench-testable before the physical switches are wired.
#pragma once

#include <Arduino.h>
#include "relays.h"

constexpr uint8_t BUTTON_COUNT = 8;   // BTN1..BTN8 (PIN_BTN[] in pins.h); 7 used + 1 spare

class Buttons {
public:
  void begin();                       // INPUT_PULLUP every BTN pin
  void update();                      // debounce; fire the action on a press edge
  void fire(uint8_t idx);             // run button idx's mapped action (0-based)

  bool    pressed(uint8_t idx) const; // current debounced state (true = pressed)
  RelayId mapping(uint8_t idx) const; // mapped relay, or RELAY_COUNT if unmapped

private:
  bool     stable_[BUTTON_COUNT]   = { false };   // debounced "pressed?" state
  bool     lastRead_[BUTTON_COUNT] = { false };   // last raw read
  uint32_t lastChange_[BUTTON_COUNT] = { 0 };     // millis of last raw change
};

extern Buttons Btns;
