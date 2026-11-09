// ===== ina3221.h — minimal TI INA3221 driver (3-ch bus + shunt voltage, I2C) =====
// Only what moto-11 needs: presence/ID check, bus voltage, shunt voltage. No
// library dependency: a handful of register reads over Wire.
//
// Bus voltage = the voltage at a channel's IN- pin relative to GND, 0-26 V,
// 8 mV/LSB. CH1 is the dedicated battery-sense lead (ADR-0011): zero current, so
// the reading is the battery terminal voltage with no IR drop.
#pragma once

#include <Arduino.h>
#include <Wire.h>

class Ina3221 {
public:
  static constexpr uint8_t ADDR_DEFAULT = 0x40;   // A0 -> GND

  // Probes the chip, checks its IDs, writes the averaging config. false = not found.
  bool begin(TwoWire& wire, uint8_t addr = ADDR_DEFAULT);
  bool present() const { return ok_; }
  uint8_t address() const { return addr_; }

  // ch = 1..3. Return false on an I2C error (value untouched).
  bool busVolts(uint8_t ch, float& v);
  bool shuntMillivolts(uint8_t ch, float& mv);

  // Raw ID registers (expected 0x5449 "TI" and 0x3220), for diagnostics.
  uint16_t manufacturerId() const { return mfr_; }
  uint16_t dieId() const { return die_; }

private:
  bool read16(uint8_t reg, uint16_t& val);
  bool write16(uint8_t reg, uint16_t val);

  TwoWire* w_   = nullptr;
  uint8_t  addr_ = ADDR_DEFAULT;
  bool     ok_  = false;
  uint16_t mfr_ = 0, die_ = 0;
};
