// ===== Native (host) unit tests for runtime config validation (ADR-0013) =====
// Exercises Config::set/get range + threshold-ladder enforcement against the live
// Supervisor tunables, with a no-op Preferences (NVS) mock. g++ on Linux/WSL:
//   bash test_host/run.sh        (from firmware/, under WSL/Linux)
#include <Arduino.h>          // mock (test_host/mock)
#include <cstdio>

#include "config.h"
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

  std::printf("\n%d checks, %d failed\n", g_count, g_fail);
  return g_fail ? 1 : 0;
}
