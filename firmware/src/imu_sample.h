// ===== imu_sample.h — one IMU reading (no Wire dependency, host-test friendly) =====
#pragma once

struct ImuSample {
  float ax, ay, az;   // g
  float gx, gy, gz;   // deg/s
  float tempC;        // die temperature (a few degC above ambient)
};
