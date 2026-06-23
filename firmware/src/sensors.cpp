// ===== sensors.cpp =====
#include "sensors.h"
#include "pins.h"
#include "log.h"

Sensors Sensor;

void Sensors::begin() {
  // IGN_SENSE is fed by the PC817 opto's pull-up (R9) in the real circuit; it
  // floats with no hardware, which is exactly why SIM mode is the default.
  pinMode(PIN_IGN_SENSE, INPUT);
  // TODO(INA3221): Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL) + ina.begin() once the
  // board/driver is in hand; then busVoltage()'s real branch returns CH1.
  LOGI("sensors: %s mode", sim_ ? "SIM" : "real");
}

float Sensors::busVoltage() {
  if (sim_) return simVbus_;
  // TODO(INA3221): return real CH1 bus voltage. Until the driver lands, the
  // real branch falls back to the sim value so nothing reads garbage.
  return simVbus_;
}

bool Sensors::ignition() {
  if (sim_) return simIgn_;
  // Opto inverts: 12 V present pulls the GPIO LOW => ignition ON.
  return digitalRead(PIN_IGN_SENSE) == LOW;
}

void Sensors::setSim(bool on) {
  sim_ = on;
  LOGI("sensors: %s mode", on ? "SIM" : "real");
}
