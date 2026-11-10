// ===== buttons.cpp =====
#include "buttons.h"
#include "button_ring.h"
#include "pins.h"
#include "log.h"
#include "loads.h"

#include <Preferences.h>
#include <stdio.h>

Buttons Btns;

namespace {
constexpr uint32_t DEBOUNCE_MS = 25;
constexpr const char* NS = "motobtn";      // NVS namespace (<= 15 chars), one key per input
constexpr uint32_t LATCH_FLAG = 0x100;     // stored value = relay code | LATCH_FLAG
constexpr const char* KEY_RING = "ring";

// Stored relay code. Schema 2 stores "unmapped" as a fixed 0xFF so adding a relay
// channel never changes what an old value means. Schema 1 (no "ver" key) stored the
// RELAY_COUNT of its day, 4, for "unmapped"; 4 is RELAY_AUX now, so it is migrated once.
constexpr const char* KEY_VER = "ver";
constexpr uint32_t SCHEMA        = 2;
constexpr uint32_t CODE_MASK     = 0xFF;
constexpr uint32_t CODE_NONE     = 0xFF;
constexpr uint32_t V1_CODE_NONE  = 4;

uint32_t encodeRelay(RelayId r) { return r == RELAY_COUNT ? CODE_NONE : static_cast<uint32_t>(r); }

// Returns the RelayId for a stored code; RELAY_COUNT = unmapped. Unknown codes come back
// as RELAY_MASTER so relayAllowed() rejects them and the default is used.
uint32_t decodeRelay(uint32_t code, bool v1) {
  if (code == CODE_NONE || (v1 && code == V1_CODE_NONE)) return RELAY_COUNT;
  return code < RELAY_COUNT ? code : RELAY_MASTER;
}
Preferences prefs;

// Ring scan timing. The 100 nF button filters recharge through the ~45 kΩ internal
// pull-up, so a released line needs far longer to rise than a driven line to fall.
constexpr uint32_t RING_SETTLE_MS = 25;
constexpr uint32_t RING_SCAN_MS   = 3;

// Case wires on the PIN_BTN inputs in ring mode. RED is the common and lands on GND.
constexpr uint8_t PIN_RING_WHITE = PIN_BTN[3];  // BTN4
constexpr uint8_t PIN_RING_BLACK = PIN_BTN[4];  // BTN5, the scan line
constexpr uint8_t PIN_RING_GREEN = PIN_BTN[5];  // BTN6

// Where the ring buttons report. Same order as the one-wire-per-button layout.
constexpr uint8_t IDX_WHITE = 3, IDX_FRONT_YELLOW = 4, IDX_REAR_RED = 5, IDX_REAR_YELLOW = 6;

// Firmware defaults + safe fallback. BTN1 is the case's latching red button.
struct DefaultAction { RelayId relay; bool latching; };
const DefaultAction DEFAULTS[BUTTON_COUNT] = {
  { RELAY_FOG,   true  },  // BTN1
  { RELAY_GRIP,  false },  // BTN2
  { RELAY_SPARE, false },  // BTN3
  { RELAY_COUNT, false },  // BTN4
  { RELAY_COUNT, false },  // BTN5
  { RELAY_COUNT, false },  // BTN6
  { RELAY_COUNT, false },  // BTN7
  { RELAY_COUNT, false },  // BTN8 (spare)
};

void nvsKey(uint8_t i, char* buf, size_t n) { snprintf(buf, n, "btn%u", i + 1); }

// MASTER is never valid — the supervisor owns it; a button must not fight the
// engine-run gating.
bool relayAllowed(uint32_t r) { return r > RELAY_MASTER && r <= RELAY_COUNT; }
}  // namespace

void Buttons::begin() {
  prefs.begin(NS, false);
  ring_       = prefs.getUInt(KEY_RING, 1) != 0;   // default: the case as supplied
  scanning_   = false;
  phaseStart_ = millis();
  bool v1 = prefs.getUInt(KEY_VER, 1) < SCHEMA;
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(PIN_BTN[i], INPUT_PULLUP);
    raw_[i]        = false;
    stable_[i]     = false;       // not pressed
    lastRead_[i]   = false;
    lastChange_[i] = millis();

    action_[i] = { DEFAULTS[i].relay, DEFAULTS[i].latching };
    char key[8];
    nvsKey(i, key, sizeof(key));
    uint32_t def = encodeRelay(DEFAULTS[i].relay) | (DEFAULTS[i].latching ? LATCH_FLAG : 0);
    uint32_t v   = prefs.getUInt(key, def);
    uint32_t r   = decodeRelay(v & CODE_MASK, v1);
    if (relayAllowed(r)) action_[i] = { static_cast<RelayId>(r), (v & LATCH_FLAG) != 0 };
    else LOGW("BTN%u: stored mapping invalid — using default", i + 1);
  }
  if (v1) {                                   // rewrite once in the schema-2 encoding
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
      char key[8];
      nvsKey(i, key, sizeof(key));
      prefs.putUInt(key, encodeRelay(action_[i].relay) | (action_[i].latching ? LATCH_FLAG : 0));
    }
    prefs.putUInt(KEY_VER, SCHEMA);
    LOGI("buttons: mapping migrated to schema %u", (unsigned)SCHEMA);
  }
}

RelayId Buttons::mapping(uint8_t i)  const { return i < BUTTON_COUNT ? action_[i].relay : RELAY_COUNT; }
bool    Buttons::pressed(uint8_t i)  const { return i < BUTTON_COUNT ? stable_[i] : false; }
bool    Buttons::latching(uint8_t i) const { return i < BUTTON_COUNT ? action_[i].latching : false; }

bool Buttons::inRing(uint8_t i) const { return ring_ && i >= IDX_WHITE && i <= IDX_REAR_YELLOW; }

void Buttons::setRing(bool on) {
  ring_ = on;
  prefs.putUInt(KEY_RING, on ? 1 : 0);
  pinMode(PIN_RING_BLACK, INPUT_PULLUP);   // never leave the scan line driven
  scanning_   = false;
  phaseStart_ = millis();
  LOGI("buttons: ring scan %s", on ? "on" : "off");
}

bool Buttons::configure(uint8_t i, RelayId relay, bool latching, const char** err) {
  auto fail = [&](const char* m) { if (err) *err = m; return false; };
  if (i >= BUTTON_COUNT) return fail("button 1-8");
  if (!relayAllowed(relay)) return fail("master is owned by the supervisor");

  action_[i] = { relay, latching };
  char key[8];
  nvsKey(i, key, sizeof(key));
  prefs.putUInt(key, encodeRelay(relay) | (latching ? LATCH_FLAG : 0));
  if (latching) act(i, stable_[i]);  // the relay takes the switch's current position now
  return true;
}

void Buttons::resetMapping() {
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) configure(i, DEFAULTS[i].relay, DEFAULTS[i].latching);
  LOGI("buttons: mapping reset to firmware defaults");
}

void Buttons::fire(uint8_t i) {
  if (i >= BUTTON_COUNT) return;
  RelayId r = action_[i].relay;
  if (r == RELAY_COUNT) { LOGI("BTN%u pressed (unmapped)", i + 1); return; }
  // Toggle the rider's intent, not the relay: a shed load is still "wanted", so a
  // press while it's held off cancels the request instead of re-asking for it.
  bool on = !Loads.wanted(r);
  Loads.request(r, on);
  LOGI("BTN%u -> %s %s", i + 1, Relays.name(r), on ? "ON" : "off");
}

void Buttons::act(uint8_t i, bool closed) {
  if (!action_[i].latching) {
    if (closed) fire(i);          // momentary: act on the press edge only
    return;
  }
  RelayId r = action_[i].relay;   // latching: the relay follows the switch position
  if (r == RELAY_COUNT) { LOGI("BTN%u %s (unmapped)", i + 1, closed ? "on" : "off"); return; }
  Loads.request(r, closed);
  LOGI("BTN%u -> %s %s", i + 1, Relays.name(r), closed ? "ON" : "off");
}

void Buttons::readRing(uint32_t now) {
  if (!scanning_) {
    if (now - phaseStart_ < RING_SETTLE_MS) return;
    idleWhite_ = digitalRead(PIN_RING_WHITE) == LOW;
    idleBlack_ = digitalRead(PIN_RING_BLACK) == LOW;
    idleGreen_ = digitalRead(PIN_RING_GREEN) == LOW;
    digitalWrite(PIN_RING_BLACK, LOW);       // set the latch first: the pin must never drive high
    pinMode(PIN_RING_BLACK, OUTPUT);
    scanning_   = true;
    phaseStart_ = now;
    return;
  }
  if (now - phaseStart_ < RING_SCAN_MS) return;
  RingSample s;
  s.idleWhite = idleWhite_;
  s.idleBlack = idleBlack_;
  s.idleGreen = idleGreen_;
  s.scanWhite = digitalRead(PIN_RING_WHITE) == LOW;
  s.scanGreen = digitalRead(PIN_RING_GREEN) == LOW;
  pinMode(PIN_RING_BLACK, INPUT_PULLUP);

  RingPressed prev = { raw_[IDX_WHITE], raw_[IDX_FRONT_YELLOW], raw_[IDX_REAR_RED], raw_[IDX_REAR_YELLOW] };
  RingPressed r = decodeRing(s, prev);
  raw_[IDX_WHITE]        = r.white;
  raw_[IDX_FRONT_YELLOW] = r.frontYellow;
  raw_[IDX_REAR_RED]     = r.rearRed;
  raw_[IDX_REAR_YELLOW]  = r.rearYellow;
  scanning_   = false;
  phaseStart_ = now;
}

void Buttons::update() {
  uint32_t now = millis();
  if (ring_) readRing(now);
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    if (!inRing(i)) raw_[i] = (digitalRead(PIN_BTN[i]) == LOW);  // active-low: pressed pulls to GND
    bool raw = raw_[i];
    if (raw != lastRead_[i]) {
      lastRead_[i]   = raw;
      lastChange_[i] = now;                        // movement: restart the debounce window
    } else if (raw != stable_[i] && now - lastChange_[i] >= DEBOUNCE_MS) {
      stable_[i] = raw;                            // settled into a new state
      act(i, raw);
    }
  }
}
