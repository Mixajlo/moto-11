// ===== buttons.h — handlebar aux buttons (ADR-0014, ADR-0016) =====
// Dry-contact buttons, active-LOW with internal pull-ups, each input debounced. What
// an input does is runtime config, saved in NVS: its relay, and whether it is momentary
// (a press toggles the relay) or latching (the switch position sets the relay). A
// different switch set is then a console change, not a firmware change. `fire()` is
// also callable from the console (`btn press <n>`) so a mapping is bench-testable
// before the switches are wired.
//
// Ring mode, also saved in NVS, reads the case's factory-wired loop through BTN4-6
// (see button_ring.h) and reports its four buttons as BTN4-7. With one wire per button
// the same four buttons land on BTN4-7 directly, so the mapping carries over.
#pragma once

#include <Arduino.h>
#include "relays.h"

constexpr uint8_t BUTTON_COUNT = 8;   // BTN1..BTN8 (PIN_BTN[] in pins.h); 7 used + 1 spare

class Buttons {
public:
  void begin();                       // INPUT_PULLUP every BTN pin, load the saved settings
  void update();                      // scan + debounce; act on a settled change
  void fire(uint8_t idx);             // toggle button idx's mapped relay (0-based)

  // Validate + persist one input's action. RELAY_COUNT = unmapped; MASTER is refused.
  bool configure(uint8_t idx, RelayId relay, bool latching, const char** err = nullptr);
  void resetMapping();                // restore the firmware default mapping + persist

  void setRing(bool on);              // select + persist ring mode (not touched by resetMapping)
  bool ring() const { return ring_; }
  bool inRing(uint8_t idx) const;     // true if idx is currently read through the ring scan

  bool    pressed(uint8_t idx) const;  // current debounced state (true = pressed/closed)
  RelayId mapping(uint8_t idx) const;  // mapped relay, or RELAY_COUNT if unmapped
  bool    latching(uint8_t idx) const; // true = switch position sets the relay

private:
  struct Action { RelayId relay; bool latching; };

  void readRing(uint32_t now);        // advance the non-blocking ring scan
  void act(uint8_t idx, bool closed); // run the action for a settled change

  Action   action_[BUTTON_COUNT];
  bool     raw_[BUTTON_COUNT]        = { false };   // latest undebounced state
  bool     stable_[BUTTON_COUNT]     = { false };   // debounced "pressed?" state
  bool     lastRead_[BUTTON_COUNT]   = { false };   // last raw read
  uint32_t lastChange_[BUTTON_COUNT] = { 0 };       // millis of last raw change

  bool     ring_       = true;
  bool     scanning_   = false;       // true while BLACK is driven low
  uint32_t phaseStart_ = 0;           // millis the current scan phase began
  bool     idleWhite_  = false;       // idle sample, kept for the decode after the scan
  bool     idleBlack_  = false;
  bool     idleGreen_  = false;
};

extern Buttons Btns;
