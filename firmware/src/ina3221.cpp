// ===== ina3221.cpp =====
#include "ina3221.h"

namespace {
constexpr uint8_t REG_CONFIG = 0x00;
constexpr uint8_t REG_MFR_ID = 0xFE;
constexpr uint8_t REG_DIE_ID = 0xFF;
constexpr uint16_t MFR_TI    = 0x5449;
constexpr uint16_t DIE_3221  = 0x3220;

// Config: all 3 channels on, 128-sample averaging, 1.1 ms bus + shunt
// conversion, continuous shunt+bus. One full refresh ~= 3 ch x 128 x 2.2 ms
// ~= 0.85 s. The averaging is the first low-pass stage against alternator
// ripple; the supervisor's hysteresis + debounce is the second.
//   15 RST | 14..12 CH1..3 EN | 11..9 AVG=100 (128) | 8..6 VBUSCT=100 (1.1 ms)
//   5..3 VSHCT=100 (1.1 ms) | 2..0 MODE=111 (shunt+bus, continuous)
constexpr uint16_t CONFIG = 0x7000 | (0b100 << 9) | (0b100 << 6) | (0b100 << 3) | 0b111;
}  // namespace

bool Ina3221::begin(TwoWire& wire, uint8_t addr) {
  w_ = &wire;
  addr_ = addr;
  ok_ = false;
  if (!read16(REG_MFR_ID, mfr_) || !read16(REG_DIE_ID, die_)) return false;
  if (mfr_ != MFR_TI || die_ != DIE_3221) return false;   // something else at this address
  ok_ = write16(REG_CONFIG, CONFIG);
  return ok_;
}

bool Ina3221::busVolts(uint8_t ch, float& v) {
  if (ch < 1 || ch > 3) return false;
  uint16_t raw;
  if (!read16(static_cast<uint8_t>(ch * 2), raw)) return false;     // 0x02/0x04/0x06
  v = (static_cast<int16_t>(raw) >> 3) * 0.008f;                     // 8 mV / LSB
  return true;
}

bool Ina3221::shuntMillivolts(uint8_t ch, float& mv) {
  if (ch < 1 || ch > 3) return false;
  uint16_t raw;
  if (!read16(static_cast<uint8_t>(ch * 2 - 1), raw)) return false; // 0x01/0x03/0x05
  mv = (static_cast<int16_t>(raw) >> 3) * 0.040f;                    // 40 uV / LSB
  return true;
}

bool Ina3221::read16(uint8_t reg, uint16_t& val) {
  w_->beginTransmission(addr_);
  w_->write(reg);
  if (w_->endTransmission(false) != 0) return false;
  if (w_->requestFrom(addr_, static_cast<uint8_t>(2)) != 2) return false;
  uint8_t hi = w_->read();
  uint8_t lo = w_->read();
  val = static_cast<uint16_t>(hi << 8 | lo);
  return true;
}

bool Ina3221::write16(uint8_t reg, uint16_t val) {
  w_->beginTransmission(addr_);
  w_->write(reg);
  w_->write(static_cast<uint8_t>(val >> 8));
  w_->write(static_cast<uint8_t>(val & 0xFF));
  return w_->endTransmission() == 0;
}
