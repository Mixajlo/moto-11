// ===== sensors.cpp =====
#include "sensors.h"
#include "pins.h"
#include "log.h"

// The INA3221 needs the Arduino Wire library. The host (g++) unit tests build
// this file against a mock Arduino.h with no I2C, so the hardware path is
// compiled only for the real target; there the real branch reads 0.0 V.
#if defined(ARDUINO)
#include <Wire.h>
#include "ina3221.h"
namespace { Ina3221 ina; }
#endif

Sensors Sensor;

namespace {
constexpr uint32_t VBUS_CACHE_MS = 100;   // the INA only refreshes every ~0.85 s anyway
}

void Sensors::begin() {
  // Both sense inputs are GPIO 34/35: input-only, NO internal pull-ups. The
  // PC817 stage's pull-up to 3.3 V (R9 on the mainboard, or the module's own)
  // holds them HIGH; unwired they float, which is why SIM is the default.
  pinMode(PIN_IGN_SENSE, INPUT);
  pinMode(PIN_START_SENSE, INPUT);
  probeIna();
  LOGI("sensors: %s mode", sim_ ? "SIM" : "real");
}

bool Sensors::probeIna() {
#if defined(ARDUINO)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  Wire.setTimeOut(20);                      // a missing/stuck device can't stall the loop
  inaOk_ = ina.begin(Wire);
  if (inaOk_) LOGI("sensors: INA3221 found at 0x%02X", ina.address());
  else        LOGW("sensors: INA3221 not found at 0x%02X (real vbus will read 0.0 V)",
                   Ina3221::ADDR_DEFAULT);
#endif
  everRead_ = false;
  return inaOk_;
}

uint8_t Sensors::inaAddress() const {
#if defined(ARDUINO)
  return ina.address();
#else
  return 0;
#endif
}

bool Sensors::inaChannel(uint8_t ch, float& busV, float& shuntMv) {
#if defined(ARDUINO)
  if (!inaOk_) return false;
  return ina.busVolts(ch, busV) && ina.shuntMillivolts(ch, shuntMv);
#else
  (void)ch; (void)busV; (void)shuntMv;
  return false;
#endif
}

float Sensors::busVoltage() {
  if (sim_) return simVbus_;
#if defined(ARDUINO)
  // 0.0 V on any failure: "engine not running" is the fail-OFF reading — the
  // supervisor will never power the box on a voltage it couldn't measure.
  if (!inaOk_) return 0.0f;
  uint32_t now = millis();
  if (!everRead_ || now - lastRead_ >= VBUS_CACHE_MS) {
    static bool failing = false;            // log transitions only, not every 100 ms
    float v;
    if (ina.busVolts(1, v)) {
      lastVbus_ = v;
      if (failing) { failing = false; LOGI("sensors: INA3221 reads OK again"); }
    } else {
      lastVbus_ = 0.0f;
      if (!failing) { failing = true; LOGW("sensors: INA3221 read failed (vbus -> 0.0 V)"); }
    }
    lastRead_ = now;
    everRead_ = true;
  }
  return lastVbus_;
#else
  return 0.0f;
#endif
}

bool Sensors::ignition() {
  if (sim_) return simIgn_;
  // Opto inverts: 12 V present pulls the GPIO LOW => ignition ON.
  return ignRawLow();
}

bool Sensors::ignRawLow()   { return digitalRead(PIN_IGN_SENSE)   == LOW; }
bool Sensors::startRawLow() { return digitalRead(PIN_START_SENSE) == LOW; }

void Sensors::setSim(bool on) {
  sim_ = on;
  LOGI("sensors: %s mode", on ? "SIM" : "real");
}
