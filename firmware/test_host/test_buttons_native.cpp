// ===== Native (host) unit tests for the handlebar buttons (ADR-0016) =====
// Fake pins and an in-memory NVS: mapping, momentary vs latching, persistence, and the
// factory button loop modelled electrically for all 16 combinations. g++ on Linux/WSL:
//   bash test_host/run.sh        (from firmware/, under WSL/Linux)
#include <Arduino.h>          // mock (test_host/mock)
#include <Preferences.h>
#include <cstdio>
#include <cstring>

#include "button_ring.h"
#include "buttons.h"
#include "loads.h"
#include "pins.h"

unsigned long _hostMillis = 0;
uint8_t _hostMode[40];
uint8_t _hostLatch[40];
void logLine(const char*, const char*, ...) {}

static int g_count = 0, g_fail = 0;
#define CHECK(cond, msg) do { ++g_count; if (!(cond)) { ++g_fail; std::printf("  FAIL: %s\n", msg); } } while (0)

// --- wiring model ---
static bool g_closed[BUTTON_COUNT];   // one wire per button: closed = wire on ground
static bool g_ringModel = false;
static RingPressed g_ring = { false, false, false, false };
static bool g_blackDrivenHigh = false;

enum Node { RED, WHITE, BLACK, GREEN, NODE_COUNT };

static bool connected(const RingPressed& p, Node from, Node to) {
  const bool links[NODE_COUNT][NODE_COUNT] = {
    /* RED   */ { false,         p.white,       false,         p.rearYellow },
    /* WHITE */ { p.white,       false,         p.frontYellow, false        },
    /* BLACK */ { false,         p.frontYellow, false,         p.rearRed    },
    /* GREEN */ { p.rearYellow,  false,         p.rearRed,     false        },
  };
  bool seen[NODE_COUNT] = { false };
  seen[from] = true;
  for (int pass = 0; pass < NODE_COUNT; pass++)
    for (int a = 0; a < NODE_COUNT; a++)
      for (int b = 0; b < NODE_COUNT; b++)
        if (seen[a] && links[a][b]) seen[b] = true;
  return seen[to];
}

int _hostRead(uint8_t pin) {
  if (!g_ringModel) {
    for (uint8_t i = 0; i < BUTTON_COUNT; i++)
      if (PIN_BTN[i] == pin) return g_closed[i] ? LOW : HIGH;
    return HIGH;
  }
  uint8_t black = PIN_BTN[4];
  bool driven = _hostMode[black] == OUTPUT;
  if (driven && _hostLatch[black] != LOW) g_blackDrivenHigh = true;
  Node n = pin == PIN_BTN[3] ? WHITE : pin == black ? BLACK : pin == PIN_BTN[5] ? GREEN : RED;
  if (n == RED) return HIGH;           // not part of the loop: released
  bool low = connected(g_ring, n, RED) || (driven && connected(g_ring, n, BLACK));
  return low ? LOW : HIGH;
}

// --- helpers ---
static void run(int ms = 200) {
  for (int i = 0; i < ms; i++) { _hostMillis++; Btns.update(); }
}
static void hold(uint8_t btn, bool closed) { g_closed[btn] = closed; run(); }
static void boot() {
  Relays.allOff();
  Loads.begin();
  LoadInputs running;              // buttons go through the load manager (ADR-0019);
  running.engineRunning = true;    // engine running = nothing held off, so a press acts
  Loads.update(running);
  Btns.begin();
  run();
}
static void clearWiring() {
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) g_closed[i] = false;
  g_ring = { false, false, false, false };
}
static bool same(const RingPressed& a, const RingPressed& b) {
  return a.white == b.white && a.frontYellow == b.frontYellow &&
         a.rearRed == b.rearRed && a.rearYellow == b.rearYellow;
}
static RingPressed reported() { return { Btns.pressed(3), Btns.pressed(4), Btns.pressed(5), Btns.pressed(6) }; }

static RingSample sample(const RingPressed& p) {
  RingSample s;
  s.idleWhite = connected(p, WHITE, RED);
  s.idleBlack = connected(p, BLACK, RED);
  s.idleGreen = connected(p, GREEN, RED);
  s.scanWhite = s.idleWhite || connected(p, WHITE, BLACK);
  s.scanGreen = s.idleGreen || connected(p, GREEN, BLACK);
  return s;
}

static void testDecode() {
  const RingPressed none = { false, false, false, false };
  for (unsigned combo = 0; combo < 16; combo++) {
    RingPressed p = { (combo & 1) != 0, (combo & 2) != 0, (combo & 4) != 0, (combo & 8) != 0 };
    int closed = p.white + p.frontYellow + p.rearRed + p.rearYellow;
    RingSample s = sample(p);
    if (closed <= 2) {
      CHECK(same(decodeRing(s, none), p), "up to 2 buttons decode exactly");
    } else {
      RingPressed held = { true, false, true, false };
      CHECK(same(decodeRing(s, held), held), "3+ buttons hold the previous state");
    }
  }
  RingSample fault = { false, true, false, true, true };
  CHECK(same(decodeRing(fault, { true, true, true, true }), none), "black fault reads as released");
}

static void testMapping() {
  g_ringModel = false;
  clearWiring();
  boot();
  Btns.setRing(false);
  run();

  CHECK(Btns.mapping(0) == RELAY_FOG && Btns.latching(0),     "BTN1 default fog, latching");
  CHECK(Btns.mapping(1) == RELAY_GRIP && !Btns.latching(1),   "BTN2 default grip, momentary");
  CHECK(Btns.mapping(3) == RELAY_COUNT,                       "BTN4 default unmapped");

  hold(0, true);  CHECK(Relays.isOn(RELAY_FOG),  "latch closed -> fog on");
  hold(0, false); CHECK(!Relays.isOn(RELAY_FOG), "latch open -> fog off");

  hold(1, true);  CHECK(Relays.isOn(RELAY_GRIP), "press -> grip on");
  hold(1, false); CHECK(Relays.isOn(RELAY_GRIP), "release keeps grip on");
  hold(1, true);  CHECK(!Relays.isOn(RELAY_GRIP), "second press -> grip off");
  hold(1, false);

  const char* e = nullptr;
  CHECK(!Btns.configure(0, RELAY_MASTER, false, &e), "master refused");
  CHECK(!Btns.configure(BUTTON_COUNT, RELAY_FOG, false, &e), "index out of range refused");
  CHECK(Btns.mapping(0) == RELAY_FOG, "refused edit changes nothing");

  CHECK(Btns.configure(3, RELAY_SPARE, false), "BTN4 -> spare accepted");
  CHECK(Btns.configure(1, RELAY_COUNT, false), "BTN2 -> none accepted");
  hold(3, true);  CHECK(Relays.isOn(RELAY_SPARE), "remapped BTN4 drives spare");
  hold(3, false);
  boot();
  CHECK(Btns.mapping(3) == RELAY_SPARE, "mapping survives reboot");
  CHECK(Btns.mapping(1) == RELAY_COUNT, "unmapped survives reboot");
  CHECK(!Btns.ring(), "ring off survives reboot");
  hold(1, true);  CHECK(!Relays.isOn(RELAY_GRIP), "unmapped button does nothing");
  hold(1, false);

  Btns.configure(4, RELAY_GRIP, true);
  g_closed[4] = true;
  Relays.allOff();
  Btns.begin();
  run();
  CHECK(Relays.isOn(RELAY_GRIP), "latch closed at boot -> relay on");
  hold(4, false);

  _hostNvs["btn1"] = RELAY_MASTER;
  boot();
  CHECK(Btns.mapping(0) == RELAY_FOG, "stored master falls back to default");

  Btns.resetMapping();
  CHECK(Btns.mapping(3) == RELAY_COUNT && Btns.mapping(1) == RELAY_GRIP, "reset restores defaults");
  CHECK(!Btns.ring(), "reset leaves the wiring mode alone");
}

static void testRingEndToEnd() {
  _hostNvs.clear();
  g_ringModel = true;
  clearWiring();
  boot();
  CHECK(Btns.ring(), "ring on by default");
  CHECK(!Btns.inRing(2) && Btns.inRing(3) && Btns.inRing(6) && !Btns.inRing(7), "ring covers BTN4-7");

  for (unsigned combo = 0; combo < 16; combo++) {
    RingPressed p = { (combo & 1) != 0, (combo & 2) != 0, (combo & 4) != 0, (combo & 8) != 0 };
    if (p.white + p.frontYellow + p.rearRed + p.rearYellow > 2) continue;
    g_ring = p;
    run();
    CHECK(same(reported(), p), "ring buttons report through the full scan");
  }
  g_ring = { false, false, false, false };
  run();

  Btns.configure(5, RELAY_SPARE, false);      // rear red: not on the common
  g_ring.rearRed = true;  run();
  CHECK(Relays.isOn(RELAY_SPARE), "rear red drives its relay");
  g_ring.rearRed = false; run();
  CHECK(Relays.isOn(RELAY_SPARE), "release keeps it on (momentary)");
  CHECK(!g_blackDrivenHigh, "black is never driven high");

  Btns.setRing(false);
  CHECK(_hostMode[PIN_BTN[4]] == INPUT_PULLUP, "ring off releases the scan line");
  uint8_t modeBefore = _hostMode[PIN_BTN[4]];
  run();
  CHECK(_hostMode[PIN_BTN[4]] == modeBefore, "scan line stays released with ring off");
  CHECK(Btns.mapping(5) == RELAY_SPARE, "mapping carries over when ring is turned off");
}

// Schema 1 (before AUX existed) stored "unmapped" as 4, which is RELAY_AUX today.
static void testMigrationAndAux() {
  _hostNvs.clear();
  g_ringModel = false;
  clearWiring();
  _hostNvs["ring"] = 0;
  _hostNvs["btn3"] = RELAY_SPARE;               // v1: spare
  _hostNvs["btn4"] = 4;                         // v1: unmapped
  _hostNvs["btn5"] = 4 | 0x100;                 // v1: unmapped, latching
  boot();
  CHECK(Btns.mapping(2) == RELAY_SPARE, "v1 spare stays spare");
  CHECK(Btns.mapping(3) == RELAY_COUNT, "v1 unmapped is not read as aux");
  CHECK(Btns.mapping(4) == RELAY_COUNT && Btns.latching(4), "v1 unmapped latching keeps its type");
  CHECK(_hostNvs["ver"] == 2, "schema version written");
  CHECK(_hostNvs["btn4"] == 0xFF, "unmapped rewritten as 0xFF");
  hold(3, true); CHECK(!Relays.isOn(RELAY_AUX), "migrated unmapped button does nothing");
  hold(3, false);

  boot();                                       // second boot: no re-migration
  CHECK(Btns.mapping(3) == RELAY_COUNT, "unmapped stable after migration");

  CHECK(Btns.configure(3, RELAY_AUX, false), "BTN4 -> aux accepted");
  boot();
  CHECK(Btns.mapping(3) == RELAY_AUX, "aux mapping survives reboot");
  hold(3, true); CHECK(Relays.isOn(RELAY_AUX), "BTN4 drives aux");
  hold(3, false);
  CHECK(Relays.pin(RELAY_AUX) == 12 && !strcmp(Relays.name(RELAY_AUX), "aux"), "aux = GPIO 12");

  _hostNvs["btn6"] = 0x42;                      // garbage code
  boot();
  CHECK(Btns.mapping(5) == RELAY_COUNT, "unknown stored code falls back to default");
}

int main() {
  testDecode();
  testMapping();
  testRingEndToEnd();
  testMigrationAndAux();
  std::printf("%d checks, %d failed\n", g_count, g_fail);
  return g_fail ? 1 : 0;
}
