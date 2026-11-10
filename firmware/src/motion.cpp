// ===== motion.cpp =====
#include "motion.h"
#include "log.h"

#include <Preferences.h>
#include <math.h>

MotionMonitor Motion;

namespace {
constexpr const char* NS = "motoimu";      // NVS namespace for the calibration
constexpr uint32_t CAL_MS        = 2000;   // calibration averaging window
constexpr uint16_t CAL_MIN_N     = 50;
constexpr float    CAL_MAX_ASD   = 0.01f;  // g, per-axis std dev (bench noise ~0.003; a 6 deg rock ~0.06)
constexpr float    CAL_MAX_GSD   = 1.0f;   // deg/s, per-axis std dev (bench noise ~0.1)
constexpr float    FILTER_K      = 0.1f;   // attitude low-pass per sample (~100 ms at 100 Hz)
constexpr float    STILL_ROT     = 5.0f;   // deg/s
constexpr float    STILL_IMPACT  = 0.05f;  // g
constexpr float    TIP_HYST      = 10.0f;  // deg: back below tipDeg - this to count upright
Preferences prefs;

float angleDeg(float ax, float ay, float az, float bx, float by, float bz) {
  float na = sqrtf(ax * ax + ay * ay + az * az), nb = sqrtf(bx * bx + by * by + bz * bz);
  if (na < 1e-6f || nb < 1e-6f) return 0.0f;
  float c = (ax * bx + ay * by + az * bz) / (na * nb);
  if (c > 1.0f) c = 1.0f;
  if (c < -1.0f) c = -1.0f;
  return acosf(c) * 57.29578f;
}
}  // namespace

const char* MotionMonitor::eventName(MotionEvent e) {
  switch (e) {
    case MEV_BUMP:            return "BUMP";
    case MEV_MOVED:           return "MOVED";
    case MEV_TIPPED:          return "TIPPED";
    case MEV_UPRIGHT:         return "UPRIGHT";
    case MEV_CRASH_PENDING:   return "CRASH_PENDING";
    case MEV_CRASH_CANCELLED: return "CRASH_CANCELLED";
    case MEV_CRASH_CONFIRMED: return "CRASH_CONFIRMED";
    default:                  return "none";
  }
}

const char* MotionMonitor::crashName(CrashState c) {
  switch (c) {
    case CR_IMPACT:    return "IMPACT";
    case CR_TIPPED:    return "TIPPED";
    case CR_PENDING:   return "PENDING";
    case CR_CONFIRMED: return "CONFIRMED";
    default:           return "none";
  }
}

void MotionMonitor::begin() {
  prefs.begin(NS, false);
  ImuCal c;
  c.valid = prefs.getUInt("ok", 0) == 1;
  if (c.valid) {
    c.gbx = prefs.getFloat("gbx", 0); c.gby = prefs.getFloat("gby", 0); c.gbz = prefs.getFloat("gbz", 0);
    c.rx  = prefs.getFloat("rx", 0);  c.ry  = prefs.getFloat("ry", 0);  c.rz  = prefs.getFloat("rz", 1);
  }
  cal_ = c;
  applyCal();
  LOGI("motion: %s", cal_.valid ? "calibration loaded" : "NOT calibrated (run 'imu cal' at rest, upright)");
}

void MotionMonitor::applyCal() {
  float n = sqrtf(cal_.rx * cal_.rx + cal_.ry * cal_.ry + cal_.rz * cal_.rz);
  if (n < 0.5f || n > 1.5f) { cal_.rx = 0; cal_.ry = 0; cal_.rz = 1; n = 1; cal_.valid = false; }
  scale_ = 1.0f / n;
  have_ = false;
  parkRefOk_ = false;
  moved_ = false;
  stillHold_.reset();
}

void MotionMonitor::persistCal() {
  prefs.putUInt("ok", cal_.valid ? 1 : 0);
  prefs.putFloat("gbx", cal_.gbx); prefs.putFloat("gby", cal_.gby); prefs.putFloat("gbz", cal_.gbz);
  prefs.putFloat("rx", cal_.rx);   prefs.putFloat("ry", cal_.ry);   prefs.putFloat("rz", cal_.rz);
}

void MotionMonitor::setCal(const ImuCal& c) {
  cal_ = c;
  applyCal();
  persistCal();
}

void MotionMonitor::startCal(uint32_t now) {
  calState_ = CAL_RUNNING;
  calErr_ = "";
  calStart_ = now;
  calN_ = 0;
  for (double& s : cs_) s = 0;
  for (double& q : cq_) q = 0;
  LOGI("motion: calibrating - keep the bike still for 2 s");
}

// Stillness is judged on the standard deviation per axis, not min-max: one bad sample
// in ~200 barely moves a std dev (and the mean by ~0.5 %), while real movement does.
void MotionMonitor::calFeed(const ImuSample& s, uint32_t now) {
  const float v[6] = { s.ax, s.ay, s.az, s.gx, s.gy, s.gz };
  for (int i = 0; i < 6; i++) { cs_[i] += v[i]; cq_[i] += double(v[i]) * v[i]; }
  calN_++;
  if (now - calStart_ < CAL_MS) return;

  float asd = 0, gsd = 0;
  for (int i = 0; i < 6; i++) {
    const double m = cs_[i] / calN_;
    double var = cq_[i] / calN_ - m * m;
    if (var < 0) var = 0;
    const float sd = sqrtf(float(var));
    if (i < 3) { if (sd > asd) asd = sd; } else if (sd > gsd) gsd = sd;
  }
  if (calN_ < CAL_MIN_N) {
    calState_ = CAL_FAILED; calErr_ = "too few samples";
  } else if (asd > CAL_MAX_ASD || gsd > CAL_MAX_GSD) {
    calState_ = CAL_FAILED; calErr_ = "it moved - keep it still and try again";
  } else {
    ImuCal c;
    c.valid = true;
    c.rx  = cs_[0] / calN_; c.ry  = cs_[1] / calN_; c.rz  = cs_[2] / calN_;
    c.gbx = cs_[3] / calN_; c.gby = cs_[4] / calN_; c.gbz = cs_[5] / calN_;
    setCal(c);
    calState_ = CAL_DONE;
    LOGI("motion: calibrated (%u samples, sd %.4f g / %.2f deg/s): up=(%.3f %.3f %.3f) g, gyro bias=(%.2f %.2f %.2f) deg/s",
         (unsigned)calN_, asd, gsd, c.rx, c.ry, c.rz, c.gbx, c.gby, c.gbz);
    return;
  }
  LOGW("motion: calibration FAILED: %s (n=%u, accel sd %.4f g [max %.3f], gyro sd %.2f deg/s [max %.1f])",
       calErr_, (unsigned)calN_, asd, CAL_MAX_ASD, gsd, CAL_MAX_GSD);
}
void MotionMonitor::emit(MotionEvent e, uint32_t now) {
  lastEv_ = e;
  lastEvAt_ = now;
  if (e < 8) counts_[e]++;
  if (e == MEV_BUMP || e == MEV_UPRIGHT || e == MEV_CRASH_CANCELLED)
    LOGI("motion: %s (tilt %.0f deg, park delta %.1f deg)", eventName(e), tilt_, parkDelta_);
  else
    LOGW("motion: %s (tilt %.0f deg, park delta %.1f deg)", eventName(e), tilt_, parkDelta_);
}

void MotionMonitor::cancelCrash(uint32_t now) {
  if (crash_ == CR_NONE) return;
  crash_ = CR_NONE;
  uprightHold_.reset();
  emit(MEV_CRASH_CANCELLED, now);
}

uint32_t MotionMonitor::crashLeftMs(uint32_t now) const {
  if (crash_ != CR_PENDING) return 0;
  uint32_t el = now - crashT_;
  return el >= cancelWindowMs ? 0 : cancelWindowMs - el;
}

void MotionMonitor::feed(const ImuSample& s, bool engineRunning, uint32_t now) {
  const float mag = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
  // Known corrupt-read signature (bench, 2026-11-10): all gyro axes identical, ~0 g.
  if (s.gx == s.gy && s.gy == s.gz && mag < 0.3f) { glitches_++; return; }
  if (calState_ == CAL_RUNNING) calFeed(s, now);

  const float gx = s.gx - cal_.gbx, gy = s.gy - cal_.gby, gz = s.gz - cal_.gbz;
  const float rot = fmaxf(fabsf(gx), fmaxf(fabsf(gy), fabsf(gz)));

  // Context: riding = running now or recently (a crash usually stalls the engine).
  if (engineRunning) { lastRunAt_ = now; ranEver_ = true; }
  const bool wasRiding = riding_;
  riding_ = engineRunning || (ranEver_ && now - lastRunAt_ < rideHoldMs);
  if (riding_ != wasRiding) { parkRefOk_ = false; moved_ = false; stillHold_.reset(); moveHold_.reset(); }

  // Attitude: low-pass the accel vector, compare with the calibrated upright.
  if (!have_) { fx_ = s.ax; fy_ = s.ay; fz_ = s.az; have_ = true; }
  else { fx_ += FILTER_K * (s.ax - fx_); fy_ += FILTER_K * (s.ay - fy_); fz_ += FILTER_K * (s.az - fz_); }
  tilt_ = angleDeg(fx_, fy_, fz_, cal_.rx, cal_.ry, cal_.rz);
  impact_ = fabsf(mag * scale_ - 1.0f);
  if (impact_ > peak_) peak_ = impact_;
  samples_++;

  // Impact needs two consecutive samples over the threshold.
  const bool overBump  = impact_ > bumpG;
  const bool overCrash = impact_ > crashG;
  const bool bumpHit  = overBump && prevImpactBump_;
  const bool crashHit = overCrash && prevImpactCrash_;
  prevImpactBump_ = overBump;
  prevImpactCrash_ = overCrash;

  // Tipped / upright (hysteresis + hold), in any context.
  const bool wantTip = tipped_ ? !(tilt_ < tipDeg - TIP_HYST) : (tilt_ > tipDeg);
  if (wantTip != tipped_) {
    if (tipHold_.held(true, now, tipHoldMs)) {
      tipped_ = wantTip;
      tipHold_.reset();
      emit(tipped_ ? MEV_TIPPED : MEV_UPRIGHT, now);
    }
  } else {
    tipHold_.reset();
  }

  // Crash state machine.
  switch (crash_) {
    case CR_NONE:
      if (riding_ && crashHit) {
        crash_ = CR_IMPACT; crashT_ = now;
        LOGW("motion: impact %.1f g while riding - watching for a fall", impact_);
      }
      break;
    case CR_IMPACT:
      if (tilt_ > tipDeg) { crash_ = CR_TIPPED; crashT_ = now; }
      else if (now - crashT_ > impactWindowMs) {
        crash_ = CR_NONE;
        LOGI("motion: impact without a fall - no crash");
      }
      break;
    case CR_TIPPED:
      if (tilt_ <= tipDeg) crash_ = CR_NONE;                 // came back up: not lying down
      else if (now - crashT_ >= crashConfirmMs) {
        crash_ = CR_PENDING; crashT_ = now; uprightHold_.reset();
        emit(MEV_CRASH_PENDING, now);
      }
      break;
    case CR_PENDING:
    case CR_CONFIRMED:
      if (uprightHold_.held(tilt_ < tipDeg - TIP_HYST, now, tipHoldMs)) {
        if (crash_ == CR_PENDING) { crash_ = CR_NONE; emit(MEV_CRASH_CANCELLED, now); }
        else { crash_ = CR_NONE; LOGI("motion: bike upright again after a confirmed crash"); }
      } else if (crash_ == CR_PENDING && now - crashT_ >= cancelWindowMs) {
        crash_ = CR_CONFIRMED;
        emit(MEV_CRASH_CONFIRMED, now);             // SMS hook (tablet LTE) goes here
      }
      break;
  }

  // Parked security: reference attitude, MOVED, BUMP. Not while a crash is open.
  if (riding_ || crash_ != CR_NONE) { parkDelta_ = 0; return; }
  const bool still = rot < STILL_ROT && impact_ < STILL_IMPACT;
  if (!parkRefOk_ || moved_) {
    if (stillHold_.held(still, now, settleMs)) {
      px_ = fx_; py_ = fy_; pz_ = fz_;
      const bool rearm = moved_;
      parkRefOk_ = true; moved_ = false; moveHold_.reset(); stillHold_.reset();
      LOGI("motion: parked reference %s - watching for movement", rearm ? "re-taken" : "set");
    }
  }
  parkDelta_ = parkRefOk_ ? angleDeg(fx_, fy_, fz_, px_, py_, pz_) : 0.0f;
  if (parkRefOk_ && !moved_ && moveHold_.held(parkDelta_ > moveDeg, now, moveHoldMs)) {
    moved_ = true;
    stillHold_.reset();
    emit(MEV_MOVED, now);
  }
  if (parkRefOk_ && bumpHit && (!bumpEver_ || now - lastBump_ >= bumpGapMs)) {
    bumpEver_ = true; lastBump_ = now;
    emit(MEV_BUMP, now);
  }
}
