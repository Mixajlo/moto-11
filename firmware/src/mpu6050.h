// ===== mpu6050.h — minimal InvenSense MPU-6050 driver (6-axis IMU, I2C) =====
// Only what moto-11 needs for now: presence/ID check, wake + config, and one burst
// read of accel / gyro / die temperature. No library dependency, same style as
// ina3221.h. Wake-on-motion (INT -> GPIO33) and the security logic come later.
//
// Module: GY-521 / HW-123 breakout (A2 on the mainboard). AD0 unconnected = 0x68
// (its pull-down); AD0 high = 0x69. The module's own regulator and 4k7 I2C pull-ups
// run from VCC = the ESP's 3V3.
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "imu_sample.h"

class Mpu6050 {
public:
  static constexpr uint8_t ADDR_DEFAULT = 0x68;   // AD0 low
  static constexpr uint8_t ADDR_ALT     = 0x69;   // AD0 high

  // Probes the chip, checks WHO_AM_I, wakes it, sets +-4 g / +-500 dps / 44 Hz DLPF.
  // false = nothing (or something else) at this address.
  bool begin(TwoWire& wire, uint8_t addr = ADDR_DEFAULT);
  bool present() const { return ok_; }
  uint8_t address() const { return addr_; }
  uint8_t whoAmI() const { return who_; }   // 0x68 genuine; clones report other values

  bool read(ImuSample& s);                  // false on an I2C error (s untouched)

private:
  bool readRegs(uint8_t reg, uint8_t* buf, uint8_t n);
  bool write8(uint8_t reg, uint8_t val);

  TwoWire* w_    = nullptr;
  uint8_t  addr_ = ADDR_DEFAULT;
  bool     ok_   = false;
  uint8_t  who_  = 0;
};
