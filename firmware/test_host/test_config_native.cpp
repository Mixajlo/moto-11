// ===== Native (host) unit tests for runtime config validation (ADR-0013) =====
// Exercises Config::set/get range + threshold-ladder enforcement against the live
// Supervisor tunables, with a no-op Preferences (NVS) mock. g++ on Linux/WSL:
//   bash test_host/run.sh        (from firmware/, under WSL/Linux)
#include <Arduino.h>          // mock (test_host/mock)
#include <cstdio>
#include <cstring>

#include "config.h"
#include "loads.h"
#include "motion.h"
#include "supervisor.h"

unsigned long _hostMillis = 0;
void logLine(const char*, const char*, ...) {}   // silence config/supervisor logging

static int g_count = 0, g_fail = 0;
#define CHECK(cond, msg) do { ++g_count; if (!(cond)) { ++g_fail; std::printf("  FAIL: %s\n", msg); } } while (0)

static float getf(const char* k) { float v = 0; Cfg.getValue(k, v); return v; }

int main() {
  Cfg.begin();   // mock NVS is empty -> Engine stays on compile-time defaults

  // defaults loaded
  CHECK(getf("vRunOn")   == 13.2f,  "default vRunOn 13.2");
  CHECK(getf("offDelay") == 30000,  "default offDelay 30000");

  // valid timer edit
  CHECK(Cfg.set("offDelay", 45000), "set offDelay 45000 accepted");
  CHECK(getf("offDelay") == 45000,  "offDelay now 45000");
  CHECK(Cfg.set("OFFDELAY", 50000), "key match is case-insensitive");
  CHECK(getf("offDelay") == 50000,  "offDelay now 50000");

  // range rejects
  const char* e = nullptr;
  CHECK(!Cfg.set("vRunOn",   11.0f,  &e), "vRunOn 11.0 rejected (range)");
  CHECK(!Cfg.set("offDelay", 999999, &e), "offDelay 999999 rejected (range)");
  CHECK(!Cfg.set("backstop", -1,     &e), "backstop -1 rejected (range)");

  // ladder rejects (vRunOff < vRunOn < chargeWarn < chargeClear)
  CHECK(!Cfg.set("vRunOff",      13.5f, &e), "vRunOff 13.5 rejected (>= vRunOn)");
  CHECK(!Cfg.set("chargeClear",  13.3f, &e), "chargeClear 13.3 rejected (<= chargeWarn)");
  CHECK(!Cfg.set("vRunOn",       13.7f, &e), "vRunOn 13.7 rejected (>= chargeWarn)");

  // unknown key
  CHECK(!Cfg.set("bogus", 1.0f, &e), "unknown key rejected");

  // rejects must not have mutated Engine
  CHECK(getf("vRunOff")     == 12.9f, "vRunOff unchanged after rejects");
  CHECK(getf("chargeClear") == 13.6f, "chargeClear unchanged after rejects");

  // valid voltage edit inside the ladder
  CHECK(Cfg.set("vRunOn", 13.25f), "vRunOn 13.25 accepted");
  CHECK(getf("vRunOn") == 13.25f,  "vRunOn now 13.25");

  // reset restores defaults (incl. the timer we changed)
  Cfg.resetDefaults();
  CHECK(getf("offDelay") == 30000, "reset -> offDelay 30000");
  CHECK(getf("vRunOn")   == 13.2f, "reset -> vRunOn 13.2");

  // ignition debounce key
  CHECK(getf("ignDebounce") == 100, "default ignDebounce 100");
  CHECK(Cfg.set("ignDebounce", 250), "ignDebounce 250 accepted");
  CHECK(getf("ignDebounce") == 250 && Engine.ignDebounceMs == 250, "ignDebounce applied to Engine");
  CHECK(!Cfg.set("ignDebounce", 5000, &e), "ignDebounce 5000 rejected (range)");
  Cfg.resetDefaults();
  CHECK(getf("ignDebounce") == 100, "reset -> ignDebounce 100");

  // load-manager keys (ADR-0019)
  CHECK(getf("shedStep") == 30000 && getf("crankHold") == 1500, "default shedStep 30000, crankHold 1500");
  CHECK(Cfg.set("shedStep", 10000) && Loads.shedStepMs == 10000, "shedStep 10000 applied to Loads");
  CHECK(!Cfg.set("shedStep", 1000, &e), "shedStep 1000 rejected (range)");
  CHECK(Cfg.set("crankHold", 0) && Loads.crankHoldMs == 0, "crankHold 0 (crank shed off) accepted");
  CHECK(!Cfg.set("crankHold", 20000, &e), "crankHold 20000 rejected (range)");
  Cfg.resetDefaults();
  CHECK(Loads.shedStepMs == 30000 && Loads.crankHoldMs == 1500, "reset -> load defaults");
  CHECK(Config::KEY_COUNT == 16 && !strcmp(Config::key(15), "crashCancel"), "16 keys, crashCancel last");

  // motion keys (ADR-0020)
  CHECK(getf("tipDeg") == 60.0f && getf("crashG") == 3.0f && getf("moveDeg") == 3.0f, "motion defaults");
  CHECK(getf("crashCancel") == 30000, "default crashCancel 30000");
  CHECK(Cfg.set("tipDeg", 55) && Motion.tipDeg == 55.0f, "tipDeg 55 applied");
  CHECK(!Cfg.set("tipDeg", 90, &e), "tipDeg 90 rejected (range)");
  CHECK(!Cfg.set("crashG", 9, &e), "crashG 9 rejected (over the 8 g IMU range)");
  CHECK(!Cfg.set("crashCancel", 5000, &e), "crashCancel 5000 rejected (too short to cancel)");
  Cfg.resetDefaults();
  CHECK(Motion.tipDeg == 60.0f && Motion.cancelWindowMs == 30000, "reset -> motion defaults");

  std::printf("\n%d checks, %d failed\n", g_count, g_fail);
  return g_fail ? 1 : 0;
}
