// ===== Native (host) unit tests for the IMU security/crash logic (ADR-0020) =====
// Synthetic IMU samples at 100 Hz, modelled on the bench unit: rest reads 1.115 g on z
// with a gyro bias of (-3.0, 1.9, 0.4) deg/s. g++ on Linux/WSL:
//   bash test_host/run.sh        (from firmware/)
#include <Arduino.h>          // mock (test_host/mock)
#include <cmath>
#include <cstdio>

#include "motion.h"

unsigned long _hostMillis = 0;
void logLine(const char*, const char*, ...) {}

static int g_count = 0, g_fail = 0;
#define CHECK(cond, msg) do { ++g_count; if (!(cond)) { ++g_fail; std::printf("  FAIL: %s\n", msg); } } while (0)

static uint32_t g_now = 0;
constexpr float REST_G = 1.115f;

// Bike rolled by `rollDeg` about x, with |a| = g * REST_G, plus the gyro bias (+ extra).
static ImuSample sample(float rollDeg, float g = 1.0f, float extraRot = 0.0f) {
  const float r = rollDeg * 3.14159265f / 180.0f;
  ImuSample s;
  s.ax = 0.0f;
  s.ay = g * REST_G * sinf(r);
  s.az = g * REST_G * cosf(r);
  s.gx = -3.0f + extraRot; s.gy = 1.9f; s.gz = 0.4f;
  s.tempC = 30.0f;
  return s;
}
static void run(const ImuSample& s, uint32_t ms, bool engine = false) {
  for (uint32_t t = 0; t < ms; t += 10) { g_now += 10; Motion.feed(s, engine, g_now); }
}
static void one(const ImuSample& s, bool engine = false) { g_now += 10; Motion.feed(s, engine, g_now); }

static void fresh() {
  Motion = MotionMonitor();          // default tunables, no state
  g_now += 100000;                   // far from any previous ride
  ImuCal c;
  c.valid = true; c.rx = 0; c.ry = 0; c.rz = REST_G; c.gbx = -3.0f; c.gby = 1.9f; c.gbz = 0.4f;
  Motion.setCal(c);
}

static void testCalibration() {
  Motion = MotionMonitor();
  Motion.startCal(g_now);
  run(sample(0), 2100);
  CHECK(Motion.calState() == MotionMonitor::CAL_DONE, "still for 2 s -> calibration done");
  CHECK(fabsf(Motion.cal().rz - REST_G) < 0.001f, "upright reference = measured rest vector");
  CHECK(fabsf(Motion.cal().gbx + 3.0f) < 0.01f && fabsf(Motion.cal().gby - 1.9f) < 0.01f, "gyro bias captured");
  run(sample(0), 300);
  CHECK(Motion.tiltDeg() < 0.5f && Motion.impactG() < 0.01f, "after cal: 0 deg tilt, 1.00 g at rest");

  Motion = MotionMonitor();
  Motion.startCal(g_now);
  for (int i = 0; i < 210; i++) one(sample(i % 2 ? 6.0f : 0.0f));
  CHECK(Motion.calState() == MotionMonitor::CAL_FAILED, "moving during cal -> rejected");

  Motion = MotionMonitor();                    // one bad gyro sample in ~200 (seen on the bench)
  Motion.startCal(g_now);
  run(sample(0), 1000);
  one(sample(0, 1.0f, 5.7f));
  run(sample(0), 1100);
  CHECK(Motion.calState() == MotionMonitor::CAL_DONE, "single outlier tolerated (std dev, not min-max)");
  CHECK(fabsf(Motion.cal().gbx + 3.0f) < 0.05f, "and barely moves the bias");
}

static void testParkedMoved() {
  fresh();
  run(sample(0), 9000);
  CHECK(!Motion.parkArmed(), "parked: reference not set before 10 s still");
  run(sample(0), 1500);
  CHECK(Motion.parkArmed() && !Motion.riding(), "after 10 s still: armed, parked");
  run(sample(2.0f), 3000);
  CHECK(!Motion.moved(), "2 deg (below moveDeg 3) -> not moved");
  run(sample(6.0f), 600);
  CHECK(!Motion.moved(), "6 deg but not yet held for 1 s");
  run(sample(6.0f), 1000);
  CHECK(Motion.moved() && Motion.count(MEV_MOVED) == 1, "6 deg held -> MOVED once");
  run(sample(6.0f), 11000);
  CHECK(!Motion.moved() && Motion.parkArmed(), "still 10 s at the new attitude -> re-armed");
  run(sample(0.0f), 2000);
  CHECK(Motion.count(MEV_MOVED) == 2, "moved back -> MOVED again");
}

static void testBumpAndGlitch() {
  fresh();
  run(sample(0), 10500);
  one(sample(0, 1.5f));
  run(sample(0), 100);
  CHECK(Motion.count(MEV_BUMP) == 0, "single knock sample -> ignored");
  one(sample(0, 1.5f)); one(sample(0, 1.5f));
  CHECK(Motion.count(MEV_BUMP) == 1, "two consecutive -> BUMP");
  run(sample(0), 1000);
  one(sample(0, 1.5f)); one(sample(0, 1.5f));
  CHECK(Motion.count(MEV_BUMP) == 1, "second knock within 5 s -> rate-limited");

  ImuSample bad = { -0.0f, -0.03f, -0.03f, -3.9f, -3.9f, -3.9f, 35.8f };  // seen on the bench
  one(bad); one(bad); one(bad);
  CHECK(Motion.glitches() == 3 && Motion.count(MEV_BUMP) == 1 && !Motion.tipped(),
        "corrupt I2C reads dropped, no events");
}

static void testTipped() {
  fresh();
  run(sample(0), 1000);
  run(sample(75.0f), 1500);
  CHECK(!Motion.tipped(), "75 deg for <2 s -> not yet tipped");
  run(sample(75.0f), 1000);
  CHECK(Motion.tipped() && Motion.count(MEV_TIPPED) == 1, "75 deg held 2 s -> TIPPED");
  run(sample(55.0f), 3000);
  CHECK(Motion.tipped(), "55 deg is inside the 10 deg hysteresis -> still tipped");
  run(sample(10.0f), 3000);
  CHECK(!Motion.tipped() && Motion.count(MEV_UPRIGHT) == 1, "back to 10 deg -> UPRIGHT");
  run(sample(15.0f), 3000);
  CHECK(!Motion.tipped(), "side-stand lean (15 deg) is not tipped");
}

static void testCrash() {
  fresh();
  run(sample(0), 2000, true);                  // riding
  CHECK(Motion.riding(), "engine running -> riding context");
  one(sample(0, 4.5f), true); one(sample(0, 4.5f), true);
  CHECK(Motion.crash() == CR_IMPACT, "4.5 g on 2 samples while riding -> IMPACT");
  run(sample(80.0f), 3600, false);              // falls; engine stalls (tip-over cut)
  CHECK(Motion.riding(), "engine stopped < rideHold ago -> still riding context");
  CHECK(Motion.crash() == CR_PENDING && Motion.count(MEV_CRASH_PENDING) == 1, "impact + lying 3 s -> PENDING");
  CHECK(Motion.crashLeftMs(g_now) > 29000, "30 s cancel window running");
  run(sample(80.0f), 30100);
  CHECK(Motion.crash() == CR_CONFIRMED && Motion.count(MEV_CRASH_CONFIRMED) == 1, "not cancelled -> CONFIRMED");
  run(sample(0.0f), 3000);
  CHECK(Motion.crash() == CR_NONE, "picked up afterwards -> crash closed");
}

static void testCrashCancelled() {
  fresh();
  run(sample(0), 1000, true);
  one(sample(0, 4.5f), true); one(sample(0, 4.5f), true);
  run(sample(80.0f), 3600, true);
  CHECK(Motion.crash() == CR_PENDING, "pending");
  run(sample(0.0f), 2500, true);
  CHECK(Motion.crash() == CR_NONE && Motion.count(MEV_CRASH_CANCELLED) == 1, "stood back up -> auto-cancelled");

  fresh();
  run(sample(0), 1000, true);
  one(sample(0, 4.5f), true); one(sample(0, 4.5f), true);
  run(sample(80.0f), 3600, true);
  Motion.cancelCrash(g_now);
  CHECK(Motion.crash() == CR_NONE && Motion.count(MEV_CRASH_CONFIRMED) == 0, "'crash cancel' -> cancelled");
  run(sample(80.0f), 31000, true);
  CHECK(Motion.count(MEV_CRASH_CONFIRMED) == 0, "no alert after a cancel");
}

static void testNotACrash() {
  fresh();
  run(sample(0), 1000, true);
  one(sample(0, 4.5f), true); one(sample(0, 4.5f), true);   // pothole
  run(sample(0), 4000, true);
  CHECK(Motion.crash() == CR_NONE && Motion.count(MEV_CRASH_PENDING) == 0, "impact, stays upright -> no crash");

  fresh();
  run(sample(0), 1000, true);
  run(sample(80.0f), 6000, true);                           // slow drop at a standstill
  CHECK(Motion.tipped() && Motion.crash() == CR_NONE, "tip without impact -> TIPPED, not a crash");

  fresh();
  run(sample(0), 1000, false);                              // parked
  one(sample(0, 4.5f)); one(sample(0, 4.5f));
  run(sample(80.0f), 6000);
  CHECK(Motion.crash() == CR_NONE && Motion.tipped(), "parked knock-over -> TIPPED, never a crash SMS");
}

int main() {
  testCalibration();
  testParkedMoved();
  testBumpAndGlitch();
  testTipped();
  testCrash();
  testCrashCancelled();
  testNotACrash();
  std::printf("\n%d checks, %d failed\n", g_count, g_fail);
  return g_fail ? 1 : 0;
}
