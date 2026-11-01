// Minimal no-op Preferences (NVS) shim for native (host) config tests.
// Config validation reads/writes the live Engine tunables, never reads back from
// NVS during set()/get(), so a do-nothing store is enough: getX() returns the
// caller's default, which sends Config::begin() down the "fresh NVS" path and
// leaves Engine on its compile-time defaults.
// With MOCK_NVS, getUInt/putUInt use a real in-memory store that outlives begin(),
// so a test can check that a value survives a reboot.
#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef MOCK_NVS
#include <map>
#include <string>
inline std::map<std::string, uint32_t> _hostNvs;
#endif

class Preferences {
public:
  bool begin(const char*, bool = false, const char* = nullptr) { return true; }
  void end() {}
  bool clear() { return true; }

  size_t   putUShort(const char*, uint16_t) { return 2; }
  uint16_t getUShort(const char*, uint16_t d = 0) { return d; }
#ifdef MOCK_NVS
  size_t   putUInt(const char* k, uint32_t v) { _hostNvs[k] = v; return 4; }
  uint32_t getUInt(const char* k, uint32_t d = 0) { auto it = _hostNvs.find(k); return it == _hostNvs.end() ? d : it->second; }
#else
  size_t   putUInt(const char*, uint32_t) { return 4; }
  uint32_t getUInt(const char*, uint32_t d = 0) { return d; }
#endif
  size_t   putFloat(const char*, float) { return 4; }
  float    getFloat(const char*, float d = 0) { return d; }
};
