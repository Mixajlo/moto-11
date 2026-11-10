// ===== mpu6050.cpp =====
#include "mpu6050.h"

namespace {
constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
constexpr uint8_t REG_CONFIG       = 0x1A;
constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;   // 14 bytes: ax ay az temp gx gy gz (big-endian)
constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
constexpr uint8_t REG_WHO_AM_I     = 0x75;

constexpr float ACCEL_LSB_PER_G   = 8192.0f;  // +-4 g
constexpr float GYRO_LSB_PER_DPS  = 65.5f;    // +-500 dps

// WHO_AM_I: 0x68 on a genuine MPU-6050. Cheap GY-521 clones often carry a 6500-family
// or relabelled die that answers 0x70/0x71/0x72/0x98 with the same register map.
bool knownId(uint8_t id) {
  return id == 0x68 || id == 0x70 || id == 0x71 || id == 0x72 || id == 0x98;
}

int16_t be16(const uint8_t* p) { return static_cast<int16_t>(p[0] << 8 | p[1]); }
}  // namespace

bool Mpu6050::begin(TwoWire& wire, uint8_t addr) {
  w_ = &wire;
  addr_ = addr;
  ok_ = false;
  if (!readRegs(REG_WHO_AM_I, &who_, 1) || !knownId(who_)) return false;
  ok_ = write8(REG_PWR_MGMT_1, 0x01)          // wake (it powers up asleep), clock = gyro X PLL
     && write8(REG_CONFIG, 0x03)              // DLPF 44 Hz: takes the engine-vibration edge off
     && write8(REG_SMPLRT_DIV, 9)             // 1 kHz / (1 + 9) = 100 Hz output rate
     && write8(REG_GYRO_CONFIG, 0x08)         // +-500 dps
     && write8(REG_ACCEL_CONFIG, 0x08);       // +-4 g (crash peaks clip, tilt/theft fine)
  return ok_;
}

bool Mpu6050::read(ImuSample& s) {
  if (!ok_) return false;
  uint8_t b[14];
  if (!readRegs(REG_ACCEL_XOUT_H, b, sizeof(b))) return false;
  s.ax = be16(b + 0) / ACCEL_LSB_PER_G;
  s.ay = be16(b + 2) / ACCEL_LSB_PER_G;
  s.az = be16(b + 4) / ACCEL_LSB_PER_G;
  s.tempC = be16(b + 6) / 340.0f + 36.53f;
  s.gx = be16(b + 8) / GYRO_LSB_PER_DPS;
  s.gy = be16(b + 10) / GYRO_LSB_PER_DPS;
  s.gz = be16(b + 12) / GYRO_LSB_PER_DPS;
  return true;
}

bool Mpu6050::readRegs(uint8_t reg, uint8_t* buf, uint8_t n) {
  w_->beginTransmission(addr_);
  w_->write(reg);
  if (w_->endTransmission(false) != 0) return false;
  if (w_->requestFrom(addr_, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = w_->read();
  return true;
}

bool Mpu6050::write8(uint8_t reg, uint8_t val) {
  w_->beginTransmission(addr_);
  w_->write(reg);
  w_->write(val);
  return w_->endTransmission() == 0;
}
