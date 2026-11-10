// ===== sensors.h — bus voltage + ignition, with bench simulation =====
// The supervisor reads the world through this layer, so it can run on the bench
// with no INA3221/opto wired: in SIM mode the values come from console-injected
// fakes (`sim vbus`, `sim ign`). Flip to real mode (`sim real`) once the
// hardware is in.
#pragma once

#include <Arduino.h>
#include "imu_sample.h"

class Sensors {
public:
  void  begin();

  float busVoltage();   // volts: INA3221 CH1 in real mode (0.0 if the INA is missing/failing); fake in sim
  bool  ignition();     // true = key on  (opto on IGN_SENSE in real mode; fake in sim)
  bool  cranking();     // true = START held (opto on START_SENSE in real mode; fake in sim)

  // --- raw diagnostics (always the real hardware, regardless of sim) ---
  bool  inaPresent() const { return inaOk_; }
  uint8_t inaAddress() const;
  bool  inaChannel(uint8_t ch, float& busV, float& shuntMv);   // ch 1..3; false = I2C error / no chip
  bool  ignRawLow();    // IGN_SENSE pin level is LOW (opto conducting = 12 V present)
  bool  startRawLow();  // START_SENSE pin level is LOW (start button / headlight feed sensed)
  bool  probeIna();     // re-run detection (after hot-plugging the module on the bench)

  // --- IMU (MPU-6050, 0x68/0x69); always real hardware ---
  bool    probeImu();
  bool    imuPresent() const { return imuOk_; }
  uint8_t imuAddress() const;
  uint8_t imuWhoAmI() const;
  bool    imuRead(ImuSample& s);   // false = I2C error / no chip

  // --- bench simulation ---
  void  setSim(bool on);
  bool  sim() const       { return sim_; }
  void  setSimVbus(float v){ simVbus_ = v; }
  void  setSimIgn(bool on) { simIgn_  = on; }
  void  setSimStart(bool on) { simStart_ = on; }

private:
  bool  sim_     = true;    // default SIM: no sensors on the bench yet
  float simVbus_ = 12.4f;   // a resting (engine-off) battery
  bool  simIgn_  = false;
  bool  simStart_ = false;

  bool     inaOk_     = false;
  bool     imuOk_     = false;
  float    lastVbus_  = 0.0f;
  uint32_t lastRead_  = 0;
  bool     everRead_  = false;
};

extern Sensors Sensor;
