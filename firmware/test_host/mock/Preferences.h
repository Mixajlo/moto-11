// Minimal no-op Preferences (NVS) shim for native (host) config tests.
// Config validation reads/writes the live Engine tunables, never reads back from
// NVS during set()/get(), so a do-nothing store is enough: getX() returns the
// caller's default, which sends Config::begin() down the "fresh NVS" path and
// leaves Engine on its compile-time defaults.
#pragma once
#include <stdint.h>
#include <stddef.h>

class Preferences {
public:
  bool begin(const char*, bool = false, const char* = nullptr) { return true; }
  void end() {}
  bool clear() { return true; }

  size_t   putUShort(const char*, uint16_t) { return 2; }
  uint16_t getUShort(const char*, uint16_t d = 0) { return d; }
  size_t   putUInt(const char*, uint32_t) { return 4; }
  uint32_t getUInt(const char*, uint32_t d = 0) { return d; }
  size_t   putFloat(const char*, float) { return 4; }
  float    getFloat(const char*, float d = 0) { return d; }
};
