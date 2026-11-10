// ===== Native (host) unit tests for the load manager (ADR-0019) =====
// Engine-only grips, the grips -> fog shed ladder with its dwell/restore steps, the
// crank hold, request-while-blocked memory, and the console-bypass catch. Fake clock;
// g++ on Linux/WSL:  bash test_host/run.sh   (from firmware/)
#include <Arduino.h>          // mock (test_host/mock)
#include <cstdio>

#include "loads.h"
#include "relays.h"

unsigned long _hostMillis = 0;
void logLine(const char*, const char*, ...) {}

static int g_count = 0, g_fail = 0;
#define CHECK(cond, msg) do { ++g_count; if (!(cond)) { ++g_fail; std::printf("  FAIL: %s\n", msg); } } while (0)

static LoadInputs g_in;

// Advance the fake clock in 10 ms loop ticks, feeding the current inputs each tick.
static void run(unsigned long ms) {
  for (unsigned long t = 0; t < ms; t += 10) { _hostMillis += 10; Loads.update(g_in); }
}
static void set(bool running, bool marginal = false, bool cranking = false) {
  g_in.engineRunning = running; g_in.chargeMarginal = marginal; g_in.cranking = cranking;
  run(10);
}
static bool grip() { return Relays.isOn(RELAY_GRIP); }
static bool fog()  { return Relays.isOn(RELAY_FOG); }

static void reset() {
  Relays.allOff();
  Loads.shedStepMs = 30000;
  Loads.crankHoldMs = 1500;
  Loads.begin();
  g_in = LoadInputs{};
  run(10);
}

static void testEngineOnlyGrips() {
  reset();
  CHECK(!grip() && !fog(), "boot: nothing on");
  Loads.request(RELAY_GRIP, true);
  CHECK(!grip() && Loads.wanted(RELAY_GRIP), "grip asked with engine off: held, remembered");
  CHECK(Loads.blockedWhy(RELAY_GRIP) == WHY_ENGINE_OFF, "reason = engine not running");
  set(true);
  CHECK(grip(), "engine starts -> grip comes on by itself");
  set(false);
  CHECK(!grip() && Loads.wanted(RELAY_GRIP), "stall (POWERED) -> grip off, still wanted");
  set(true);
  CHECK(grip(), "restart -> grip back on");

  reset();
  Loads.request(RELAY_FOG, true);
  CHECK(fog(), "fog is not engine-only: on with engine off");
}

static void testShedLadder() {
  reset();
  set(true);
  Loads.request(RELAY_GRIP, true);
  Loads.request(RELAY_FOG, true);
  CHECK(grip() && fog(), "running: grip + fog on");

  set(true, true);
  CHECK(!grip() && fog() && Loads.shedLevel() == 1, "marginal -> grip shed at once, fog stays");
  run(29000);
  CHECK(fog(), "fog kept for most of one shedStep");
  run(1100);
  CHECK(!fog() && Loads.shedLevel() == 2, "still marginal after shedStep -> fog shed too");

  set(true, false);                      // recovered
  run(29000);
  CHECK(!fog() && !grip(), "recovered: nothing restored before one shedStep");
  run(1100);
  CHECK(fog() && !grip() && Loads.shedLevel() == 1, "after shedStep: fog restored first");
  run(30100);
  CHECK(fog() && grip() && Loads.shedLevel() == 0, "after another shedStep: grip restored");
}

static void testSkipsLoadsThatAreOff() {
  reset();
  set(true);
  Loads.request(RELAY_FOG, true);        // grips off: shedding them would gain nothing
  set(true, true);
  CHECK(!fog() && Loads.shedLevel() == 2, "only fog on: fog shed at the first step");
  Loads.request(RELAY_GRIP, true);
  CHECK(!grip() && (Loads.blockedWhy(RELAY_GRIP) & WHY_CHARGE), "grip asked while shed: held");
  set(true, false);
  run(30100);
  CHECK(fog() && !grip(), "restore: fog first");
  run(30100);
  CHECK(grip(), "then the grip that was asked for while shed");
}

static void testCancelWhileShed() {
  reset();
  set(true);
  Loads.request(RELAY_GRIP, true);
  set(true, true);
  CHECK(!grip(), "grip shed");
  Loads.request(RELAY_GRIP, false);      // rider presses again: cancel
  set(true, false);
  run(70000);
  CHECK(!grip() && !Loads.wanted(RELAY_GRIP), "cancelled grip is not restored");
}

static void testEngineStopClearsLadder() {
  reset();
  set(true);
  Loads.request(RELAY_GRIP, true);
  Loads.request(RELAY_FOG, true);
  set(true, true);
  run(30100);
  CHECK(Loads.shedLevel() == 2, "both shed");
  set(false);
  CHECK(Loads.shedLevel() == 0 && fog() && !grip(), "engine stops: ladder cleared, fog back, grip engine-off");
}

static void testCrank() {
  reset();
  set(true);
  Loads.request(RELAY_GRIP, true);
  Loads.request(RELAY_FOG, true);
  set(true, false, true);
  CHECK(!grip() && !fog(), "cranking: grip + fog off");
  run(3000);
  CHECK(!fog(), "still cranking: still off");
  set(true, false, false);
  run(1400);
  CHECK(!fog(), "released: held for crankHold");
  run(200);
  CHECK(fog() && grip(), "after crankHold: both back");

  reset();
  Loads.crankHoldMs = 0;                 // crank shed disabled
  Loads.request(RELAY_FOG, true);
  set(false, false, true);
  CHECK(fog(), "crankHold 0: cranking sheds nothing");
}

static void testConsoleBypass() {
  reset();
  Relays.set(RELAY_FOG, true);           // console 'on fog' bypasses the manager
  run(1000);
  CHECK(fog(), "raw console on is left alone while nothing blocks it");
  set(false, false, true);
  CHECK(!fog() && Loads.wanted(RELAY_FOG), "new block (crank) sheds it and remembers it");
  set(false, false, false);
  run(1600);
  CHECK(fog(), "and it is restored with the rest");

  reset();
  Relays.set(RELAY_GRIP, true);          // relay test in SLEEP: engine-off was already there
  run(1000);
  CHECK(grip(), "no NEW reason -> console grip stays on (bench relay test works)");
}

static void testPassThrough() {
  reset();
  Loads.request(RELAY_SPARE, true);
  CHECK(Relays.isOn(RELAY_SPARE) && Loads.wanted(RELAY_SPARE), "spare passes straight through");
  Loads.request(RELAY_MASTER, true);
  CHECK(!Relays.isOn(RELAY_MASTER), "master request ignored (supervisor-owned)");
}

int main() {
  testEngineOnlyGrips();
  testShedLadder();
  testSkipsLoadsThatAreOff();
  testCancelWhileShed();
  testEngineStopClearsLadder();
  testCrank();
  testConsoleBypass();
  testPassThrough();
  std::printf("\n%d checks, %d failed\n", g_count, g_fail);
  return g_fail ? 1 : 0;
}
