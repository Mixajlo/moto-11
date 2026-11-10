// ===== motion.h — IMU security/crash logic (ADR-0020) =====
// Pure logic on IMU samples, so it is unit-tested on the host with synthetic motion.
// main.cpp polls the MPU-6050 (~100 Hz) and feeds every sample here, with whether the
// engine is RUNNING.
//
// Calibration (`imu cal`, bike at rest, upright on its normal stand position): averages
// ~2 s, rejects the run if anything moved, and stores the gyro bias and the "upright"
// gravity vector (which also absorbs the mounting angle and the accel offset in that
// pose) in NVS. Impact is measured as |a| scaled so the calibrated rest reads 1.00 g.
//
// Two contexts:
//   riding  = engine RUNNING now or within the last rideHoldMs (a crash usually stalls
//             the engine or trips the factory tip-over cut, which drops RUNNING).
//   parked  = otherwise.
//
// Events (each logged; counters + last event kept for the console / later BLE queue):
//   BUMP     parked: a knock (impact > bumpG on 2 consecutive samples), rate-limited.
//   MOVED    parked: the bike's attitude left the parked reference by > moveDeg for
//            moveHoldMs (the reference is re-taken once the bike has been still for
//            settleMs). Latched until re-armed by a new reference.
//   TIPPED   any time: > tipDeg from upright for tipHoldMs. Clears when back within
//            tipDeg - 10 for tipHoldMs.
//   CRASH    riding: impact > crashG, then tipped (> tipDeg) within impactWindowMs and held
//            for crashConfirmMs -> PENDING, a cancelWindowMs countdown -> CONFIRMED (the
//            SMS hook). Cancelled by `crash cancel` or by the bike standing upright again
//            for tipHoldMs.
//
// Single samples are never trusted: impact needs 2 consecutive samples, attitude uses a
// low-pass filtered vector with a hold time, and a known I2C-glitch signature (all three
// gyro axes identical and |a| < 0.3 g) is dropped.
#pragma once

#include <Arduino.h>
#include "imu_sample.h"

enum MotionEvent : uint8_t {
  MEV_NONE = 0, MEV_BUMP, MEV_MOVED, MEV_TIPPED, MEV_UPRIGHT,
  MEV_CRASH_PENDING, MEV_CRASH_CANCELLED, MEV_CRASH_CONFIRMED,
};

enum CrashState : uint8_t { CR_NONE, CR_IMPACT, CR_TIPPED, CR_PENDING, CR_CONFIRMED };

struct ImuCal {
  bool  valid = false;
  float gbx = 0, gby = 0, gbz = 0;     // gyro bias, deg/s
  float rx = 0, ry = 0, rz = 1;        // upright gravity reference (raw g, as measured)
};

class MotionMonitor {
public:
  // Tunables (persisted by Config, ADR-0013).
  float    moveDeg         = 3.0f;     // parked attitude change that counts as moved
  float    tipDeg          = 60.0f;    // from upright: lying down (side stand is ~10-15)
  float    crashG          = 3.0f;     // impact that can start a crash
  float    bumpG           = 0.15f;    // parked knock
  uint32_t cancelWindowMs  = 30000;    // crash pending -> confirmed

  // Fixed timings (not worth a config key yet).
  uint32_t moveHoldMs      = 1000;
  uint32_t settleMs        = 10000;
  uint32_t tipHoldMs       = 2000;
  uint32_t impactWindowMs  = 3000;
  uint32_t crashConfirmMs  = 3000;
  uint32_t rideHoldMs      = 10000;
  uint32_t bumpGapMs       = 5000;

  void begin();                        // load calibration from NVS
  void feed(const ImuSample& raw, bool engineRunning, uint32_t now);

  // Calibration: start collecting; feed() finishes it. Result via calState().
  void startCal(uint32_t now);
  enum CalState : uint8_t { CAL_IDLE, CAL_RUNNING, CAL_DONE, CAL_FAILED };
  CalState calState() const { return calState_; }
  const char* calError() const { return calErr_; }
  const ImuCal& cal() const { return cal_; }
  void setCal(const ImuCal& c);        // also persists (tests use it to skip a cal run)

  void cancelCrash(uint32_t now);      // rider cancel during PENDING
  void noteWake() { wakes_++; }        // IMU wake-on-motion INT fired (deep-sleep wake later)
  uint32_t wakes() const { return wakes_; }

  // --- status ---
  bool        riding() const { return riding_; }
  float       tiltDeg() const { return tilt_; }          // filtered, from upright
  float       parkDeltaDeg() const { return parkDelta_; } // filtered, from parked reference
  bool        parkArmed() const { return parkRefOk_; }
  float       impactG() const { return impact_; }         // last sample, |a| - 1 g
  bool        tipped() const { return tipped_; }
  bool        moved() const { return moved_; }
  CrashState  crash() const { return crash_; }
  uint32_t    crashLeftMs(uint32_t now) const;
  MotionEvent lastEvent() const { return lastEv_; }
  uint32_t    lastEventAt() const { return lastEvAt_; }
  uint32_t    count(MotionEvent e) const { return e < 8 ? counts_[e] : 0; }
  uint32_t    glitches() const { return glitches_; }
  float       takePeakImpact() { float p = peak_; peak_ = 0; return p; }  // max |a|-1 g since last call
  uint32_t    samples() const { return samples_; }                       // fed so far (rate check)
  static const char* eventName(MotionEvent e);
  static const char* crashName(CrashState c);

private:
  // "condition has held continuously for `need` ms" timer.
  struct Hold {
    bool active = false;
    uint32_t since = 0;
    bool held(bool cond, uint32_t now, uint32_t need) {
      if (!cond) { active = false; return false; }
      if (!active) { active = true; since = now; }
      return now - since >= need;
    }
    void reset() { active = false; }
  };

  void emit(MotionEvent e, uint32_t now);
  void calFeed(const ImuSample& s, uint32_t now);
  void persistCal();
  void applyCal();

  ImuCal   cal_;
  CalState calState_ = CAL_IDLE;
  const char* calErr_ = "";
  uint32_t calStart_ = 0;
  uint16_t calN_ = 0;
  double   cs_[6] = { 0 };               // sums: ax ay az gx gy gz
  double   cq_[6] = { 0 };               // sums of squares (std dev = stillness check)
  Hold     stillHold_, moveHold_, tipHold_, uprightHold_;

  bool     have_ = false;                // filter primed
  float    fx_ = 0, fy_ = 0, fz_ = 1;    // low-pass accel
  float    scale_ = 1.0f;                // 1 / |upright ref|
  float    tilt_ = 0, parkDelta_ = 0, impact_ = 0;
  bool     prevImpactBump_ = false, prevImpactCrash_ = false;
  uint32_t lastBump_ = 0;
  bool     bumpEver_ = false;

  bool     riding_ = false;
  uint32_t lastRunAt_ = 0;
  bool     ranEver_ = false;

  // parked reference
  bool     parkRefOk_ = false;
  float    px_ = 0, py_ = 0, pz_ = 1;
  bool     moved_ = false;

  // tipped
  bool     tipped_ = false;

  // crash
  CrashState crash_ = CR_NONE;
  uint32_t crashT_ = 0;                  // impact time / tipped-since / pending start

  MotionEvent lastEv_ = MEV_NONE;
  uint32_t lastEvAt_ = 0;
  uint32_t counts_[8] = { 0 };
  uint32_t glitches_ = 0;
  uint32_t wakes_ = 0;
  float    peak_ = 0;
  uint32_t samples_ = 0;
};

extern MotionMonitor Motion;
