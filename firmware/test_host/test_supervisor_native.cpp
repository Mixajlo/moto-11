// ===== Native (host) unit tests for the engine-run supervisor =====
// Runs under g++ on Linux/WSL with a fake clock, so the real production tunables
// (incl. the 10-min backstop) resolve instantly. No ESP, no PlatformIO needed.
//   bash test_host/run.sh        (from firmware/, under WSL/Linux)
#include <Arduino.h>          // mock (test_host/mock)
#include <cstdio>
#include <cstdarg>

#include "supervisor.h"
#include "sensors.h"
#include "relays.h"

unsigned long _hostMillis = 0;
void logLine(const char*, const char*, ...) {}   // silence supervisor logging in tests

static int g_count = 0, g_fail = 0;
#define CHECK(cond, msg) do { ++g_count; if (!(cond)) { ++g_fail; std::printf("  FAIL: %s\n", msg); } } while (0)

// Advance the fake clock, pumping update() like the main loop.
static void tick(unsigned long ms) {
  unsigned long target = _hostMillis + ms;
  while (_hostMillis < target) { _hostMillis += 10; Engine.update(); }
  Engine.update();
}

static void reset() {
  _hostMillis = 0;
  Relays.begin();
  Sensor.begin();
  Sensor.setSim(true);
  Sensor.setSimIgn(false);
  Sensor.setSimVbus(12.4f);
  // Real production tunables — the fake clock makes even 10 min instant.
  Engine.runDebounceMs    = 1000;
  Engine.stallDebounceMs  = 2000;
  Engine.offDelayMs       = 30000;
  Engine.backstopMs       = 600000;
  Engine.chargeDebounceMs = 5000;
  Engine.begin();
  Engine.update();
}

static void toRunning() {
  Sensor.setSimIgn(true);
  Sensor.setSimVbus(13.5f);
  tick(1500);
}

int main() {
  reset(); Sensor.setSimIgn(true); Engine.update();
  CHECK(Engine.state() == SUP_ARMED, "SLEEP -> ARMED on ignition");

  reset(); toRunning();
  CHECK(Engine.state() == SUP_RUNNING, "ARMED -> RUNNING after debounce");
  CHECK(Relays.isOn(RELAY_MASTER), "MASTER on in RUNNING");

  reset(); Sensor.setSimIgn(true); Sensor.setSimVbus(12.5f); tick(1500);
  CHECK(Engine.state() == SUP_ARMED, "stays ARMED below V_RUN_ON");

  reset(); toRunning(); Sensor.setSimVbus(12.5f); tick(2500);
  CHECK(Engine.state() == SUP_POWERED, "RUNNING -> POWERED on stall (key on)");
  CHECK(Relays.isOn(RELAY_MASTER), "MASTER held in POWERED");

  reset(); toRunning(); Sensor.setSimVbus(12.5f); tick(2500); Sensor.setSimVbus(13.5f); tick(1500);
  CHECK(Engine.state() == SUP_RUNNING, "POWERED -> RUNNING on restart");

  reset(); toRunning(); Sensor.setSimVbus(12.5f); tick(2500); tick(Engine.backstopMs + 2000);
  CHECK(Engine.state() == SUP_ARMED, "POWERED backstop -> ARMED");
  CHECK(!Relays.isOn(RELAY_MASTER), "MASTER off after backstop");

  reset(); toRunning(); Sensor.setSimIgn(false); Engine.update();
  CHECK(Engine.state() == SUP_OFF_DELAY, "key off -> OFF_DELAY");
  CHECK(Relays.isOn(RELAY_MASTER), "MASTER held during OFF_DELAY");
  tick(Engine.offDelayMs + 2000);
  CHECK(Engine.state() == SUP_SLEEP, "OFF_DELAY -> SLEEP");
  CHECK(!Relays.isOn(RELAY_MASTER), "MASTER off after OFF_DELAY");

  reset(); toRunning(); Sensor.setSimIgn(false); Engine.update();
  Sensor.setSimIgn(true); Sensor.setSimVbus(13.5f); tick(500);
  CHECK(Engine.state() == SUP_RUNNING, "OFF_DELAY resume -> RUNNING");

  reset(); toRunning();
  Sensor.setSimVbus(13.3f); tick(Engine.chargeDebounceMs / 2);
  Sensor.setSimVbus(13.7f); tick(500);
  CHECK(!Engine.chargeMarginal(), "transient dip does not warn");

  reset(); toRunning(); Sensor.setSimVbus(13.3f); tick(Engine.chargeDebounceMs + 1000);
  CHECK(Engine.chargeMarginal(), "sustained low -> marginal");

  reset(); toRunning(); Sensor.setSimVbus(13.3f); tick(Engine.chargeDebounceMs + 1000);
  CHECK(Engine.chargeMarginal(), "marginal set");
  Sensor.setSimVbus(13.5f); tick(Engine.chargeDebounceMs + 1000);
  CHECK(Engine.chargeMarginal(), "13.5 deadband holds marginal");
  Sensor.setSimVbus(13.7f); tick(Engine.chargeDebounceMs + 1000);
  CHECK(!Engine.chargeMarginal(), "13.7 clears marginal");

  std::printf("\n%d checks, %d failed\n", g_count, g_fail);
  return g_fail ? 1 : 0;
}
