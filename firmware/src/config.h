// ===== config.h — runtime-configurable tunables, persisted in NVS (ADR-0013) =====
// Operates directly on the live Supervisor (Engine) tunables: firmware initializers
// are the defaults, NVS overrides them at boot, and edits are validated + saved.
// Edited via the console now (`config` / `get` / `set` / `config reset`); BLE/app
// front-ends land later on this same backend.
#pragma once

#include <Arduino.h>

class Config {
public:
  void begin();                                  // capture defaults, load NVS, validate, apply

  // Set a tunable by name (case-insensitive). Validates range + the threshold
  // ladder; on reject returns false and (if err) a short reason. Persists on success.
  bool set(const char* key, float value, const char** err = nullptr);
  bool getValue(const char* key, float& out) const;
  void resetDefaults();                          // restore firmware defaults + persist

  static constexpr int KEY_COUNT = 9;
  static const char* key(int i);                 // canonical key name, i in [0, KEY_COUNT)

private:
  void captureDefaults();
  void applyDefaults();
  void persistAll();

  struct Def {
    float    vRunOn, vRunOff, chargeWarn, chargeClear;
    uint32_t runDebounce, stallDebounce, offDelay, backstop, chargeDebounce;
  } def_;
};

extern Config Cfg;
