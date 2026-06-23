// ===== sensors.h — bus voltage + ignition, with bench simulation =====
// The supervisor reads the world through this layer, so it can run on the bench
// with no INA3221/opto wired: in SIM mode the values come from console-injected
// fakes (`sim vbus`, `sim ign`). Flip to real mode once the hardware is in.
#pragma once

#include <Arduino.h>

class Sensors {
public:
  void  begin();

  float busVoltage();   // volts (INA3221 CH1 in real mode; fake in sim)
  bool  ignition();     // true = key on  (opto on IGN_SENSE in real mode; fake in sim)

  // --- bench simulation ---
  void  setSim(bool on);
  bool  sim() const       { return sim_; }
  void  setSimVbus(float v){ simVbus_ = v; }
  void  setSimIgn(bool on) { simIgn_  = on; }

private:
  bool  sim_     = true;    // default SIM: no sensors on the bench yet
  float simVbus_ = 12.4f;   // a resting (engine-off) battery
  bool  simIgn_  = false;
};

extern Sensors Sensor;
