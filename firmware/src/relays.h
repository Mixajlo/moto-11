// ===== relays.h — active-HIGH relay coil control =====
// One object owns the four ULN2803-driven coil enables. It is the only thing
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
  RELAY_SPARE,        // reserved aux
  RELAY_COUNT
};

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
  bool state_[RELAY_COUNT] = { false, false, false, false };
};

// Single shared instance (defined in relays.cpp).
extern RelayController Relays;
