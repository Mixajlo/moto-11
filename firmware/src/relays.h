// ===== relays.h — active-HIGH relay coil control =====
// One object owns the five ULN2803-driven coil enables (ADR-0018). It is the only thing
// that touches those GPIOs, so "relay state" has a single source of truth.
//
// Fail-OFF by construction: begin() drives every line LOW before enabling the
// output, and the channels are GPIOs that are already low at boot.
#pragma once

#include <Arduino.h>

enum RelayId : uint8_t {
  RELAY_MASTER = 0,   // external master relay — gates the whole accessory box
  RELAY_FOG,          // fog lights
  RELAY_GRIP,         // heated grips (future)
  RELAY_SPARE,        // box relay 3, spare load
  RELAY_AUX,          // box relay 4, aux load (GPIO 12)
  RELAY_COUNT
};
// Never persist a RelayId's raw value as "none" via RELAY_COUNT: it moves whenever a
// channel is added. Storage uses its own fixed encoding (see buttons.cpp).

class RelayController {
public:
  // Configure all channels as outputs and force them OFF. Call once, early.
  void begin();

  void set(RelayId id, bool on);
  bool isOn(RelayId id) const;

  void allOff();
  bool anyOn() const;

  // Human-readable channel name ("master", "fog", ...) and its GPIO number.
  const char* name(RelayId id) const;
  uint8_t     pin(RelayId id) const;

private:
  bool state_[RELAY_COUNT] = { false };
};

// Single shared instance (defined in relays.cpp).
extern RelayController Relays;
