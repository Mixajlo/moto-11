// ===== config.cpp =====
#include "config.h"
#include "supervisor.h"
#include "loads.h"
#include "motion.h"
#include "log.h"

#include <Preferences.h>
#include <string.h>
#include <strings.h>   // strcasecmp (host builds; Arduino already provides it on the ESP)

Config Cfg;

namespace {
constexpr uint16_t CONFIG_VERSION = 1;     // bump when a key's meaning changes. Adding a key
                                           // needs no bump: a missing key loads its default.
constexpr const char* NS = "motocfg";      // NVS namespace (<= 15 chars)
Preferences prefs;

const char* const KEYS[Config::KEY_COUNT] = {
  "vRunOn", "vRunOff", "chargeWarn", "chargeClear",
  "runDebounce", "stallDebounce", "offDelay", "backstop", "chargeDebounce", "ignDebounce",
  "shedStep", "crankHold",
  "moveDeg", "tipDeg", "crashG", "crashCancel"
};

bool ladderOk(float off, float on, float warn, float clear) {
  return off < on && on < warn && warn < clear;
}
}  // namespace

const char* Config::key(int i) { return (i >= 0 && i < KEY_COUNT) ? KEYS[i] : ""; }

void Config::captureDefaults() {
  def_ = { Engine.vRunOn, Engine.vRunOff, Engine.chargeWarn, Engine.chargeClear,
           Engine.runDebounceMs, Engine.stallDebounceMs, Engine.offDelayMs,
           Engine.backstopMs, Engine.chargeDebounceMs, Engine.ignDebounceMs,
           Loads.shedStepMs, Loads.crankHoldMs,
           Motion.moveDeg, Motion.tipDeg, Motion.crashG, Motion.cancelWindowMs };
}

void Config::applyDefaults() {
  Engine.vRunOn = def_.vRunOn; Engine.vRunOff = def_.vRunOff;
  Engine.chargeWarn = def_.chargeWarn; Engine.chargeClear = def_.chargeClear;
  Engine.runDebounceMs = def_.runDebounce; Engine.stallDebounceMs = def_.stallDebounce;
  Engine.offDelayMs = def_.offDelay; Engine.backstopMs = def_.backstop;
  Engine.chargeDebounceMs = def_.chargeDebounce;
  Engine.ignDebounceMs = def_.ignDebounce;
  Loads.shedStepMs = def_.shedStep; Loads.crankHoldMs = def_.crankHold;
  Motion.moveDeg = def_.moveDeg; Motion.tipDeg = def_.tipDeg;
  Motion.crashG = def_.crashG; Motion.cancelWindowMs = def_.crashCancel;
}

void Config::persistAll() {
  prefs.putFloat("vRunOn", Engine.vRunOn);
  prefs.putFloat("vRunOff", Engine.vRunOff);
  prefs.putFloat("chargeWarn", Engine.chargeWarn);
  prefs.putFloat("chargeClear", Engine.chargeClear);
  prefs.putUInt("runDebounce", Engine.runDebounceMs);
  prefs.putUInt("stallDebounce", Engine.stallDebounceMs);
  prefs.putUInt("offDelay", Engine.offDelayMs);
  prefs.putUInt("backstop", Engine.backstopMs);
  prefs.putUInt("chargeDebounce", Engine.chargeDebounceMs);
  prefs.putUInt("ignDebounce", Engine.ignDebounceMs);
  prefs.putUInt("shedStep", Loads.shedStepMs);
  prefs.putUInt("crankHold", Loads.crankHoldMs);
  prefs.putFloat("moveDeg", Motion.moveDeg);
  prefs.putFloat("tipDeg", Motion.tipDeg);
  prefs.putFloat("crashG", Motion.crashG);
  prefs.putUInt("crashCancel", Motion.cancelWindowMs);
}

void Config::begin() {
  captureDefaults();                 // snapshot the compile-time defaults from Engine
  prefs.begin(NS, false);

  if (prefs.getUShort("ver", 0) != CONFIG_VERSION) {
    prefs.clear();
    prefs.putUShort("ver", CONFIG_VERSION);
    persistAll();                    // first run / schema change -> write defaults
    LOGI("config: fresh NVS (v%u), defaults written", CONFIG_VERSION);
    return;
  }

  Engine.vRunOn        = prefs.getFloat("vRunOn", Engine.vRunOn);
  Engine.vRunOff       = prefs.getFloat("vRunOff", Engine.vRunOff);
  Engine.chargeWarn    = prefs.getFloat("chargeWarn", Engine.chargeWarn);
  Engine.chargeClear   = prefs.getFloat("chargeClear", Engine.chargeClear);
  Engine.runDebounceMs   = prefs.getUInt("runDebounce", Engine.runDebounceMs);
  Engine.stallDebounceMs = prefs.getUInt("stallDebounce", Engine.stallDebounceMs);
  Engine.offDelayMs      = prefs.getUInt("offDelay", Engine.offDelayMs);
  Engine.backstopMs      = prefs.getUInt("backstop", Engine.backstopMs);
  Engine.chargeDebounceMs= prefs.getUInt("chargeDebounce", Engine.chargeDebounceMs);
  Engine.ignDebounceMs   = prefs.getUInt("ignDebounce", Engine.ignDebounceMs);
  Loads.shedStepMs       = prefs.getUInt("shedStep", Loads.shedStepMs);
  Loads.crankHoldMs      = prefs.getUInt("crankHold", Loads.crankHoldMs);
  Motion.moveDeg         = prefs.getFloat("moveDeg", Motion.moveDeg);
  Motion.tipDeg          = prefs.getFloat("tipDeg", Motion.tipDeg);
  Motion.crashG          = prefs.getFloat("crashG", Motion.crashG);
  Motion.cancelWindowMs  = prefs.getUInt("crashCancel", Motion.cancelWindowMs);

  if (!ladderOk(Engine.vRunOff, Engine.vRunOn, Engine.chargeWarn, Engine.chargeClear)) {
    LOGW("config: stored thresholds inconsistent — restoring defaults");
    applyDefaults();
    persistAll();
  } else {
    LOGI("config: loaded from NVS (v%u)", CONFIG_VERSION);
  }
}

bool Config::set(const char* k, float v, const char** err) {
  auto fail = [&](const char* m) { if (err) *err = m; return false; };

  // --- timers (independent, range-checked) ---
  if (!strcasecmp(k, "runDebounce"))   { if (v < 0 || v > 10000)   return fail("0..10000 ms");
    Engine.runDebounceMs = (uint32_t)v;   prefs.putUInt("runDebounce", Engine.runDebounceMs); return true; }
  if (!strcasecmp(k, "stallDebounce")) { if (v < 0 || v > 30000)   return fail("0..30000 ms");
    Engine.stallDebounceMs = (uint32_t)v; prefs.putUInt("stallDebounce", Engine.stallDebounceMs); return true; }
  if (!strcasecmp(k, "offDelay"))      { if (v < 0 || v > 600000)  return fail("0..600000 ms");
    Engine.offDelayMs = (uint32_t)v;      prefs.putUInt("offDelay", Engine.offDelayMs); return true; }
  if (!strcasecmp(k, "backstop"))      { if (v < 0 || v > 3600000) return fail("0..3600000 ms (0=off)");
    Engine.backstopMs = (uint32_t)v;      prefs.putUInt("backstop", Engine.backstopMs); return true; }
  if (!strcasecmp(k, "chargeDebounce")){ if (v < 0 || v > 60000)   return fail("0..60000 ms");
    Engine.chargeDebounceMs = (uint32_t)v; prefs.putUInt("chargeDebounce", Engine.chargeDebounceMs); return true; }
  if (!strcasecmp(k, "ignDebounce"))   { if (v < 0 || v > 2000)    return fail("0..2000 ms");
    Engine.ignDebounceMs = (uint32_t)v;   prefs.putUInt("ignDebounce", Engine.ignDebounceMs); return true; }
  if (!strcasecmp(k, "shedStep"))      { if (v < 5000 || v > 600000) return fail("5000..600000 ms");
    Loads.shedStepMs = (uint32_t)v;       prefs.putUInt("shedStep", Loads.shedStepMs); return true; }
  if (!strcasecmp(k, "crankHold"))     { if (v < 0 || v > 10000)    return fail("0..10000 ms (0=no crank shed)");
    Loads.crankHoldMs = (uint32_t)v;      prefs.putUInt("crankHold", Loads.crankHoldMs); return true; }
  if (!strcasecmp(k, "crashCancel"))   { if (v < 10000 || v > 120000) return fail("10000..120000 ms");
    Motion.cancelWindowMs = (uint32_t)v;  prefs.putUInt("crashCancel", Motion.cancelWindowMs); return true; }

  // --- motion thresholds (ADR-0020) ---
  if (!strcasecmp(k, "moveDeg")) { if (v < 1.0f || v > 20.0f)  return fail("1..20 deg");
    Motion.moveDeg = v; prefs.putFloat("moveDeg", v); return true; }
  if (!strcasecmp(k, "tipDeg"))  { if (v < 30.0f || v > 85.0f) return fail("30..85 deg");
    Motion.tipDeg = v;  prefs.putFloat("tipDeg", v);  return true; }
  if (!strcasecmp(k, "crashG"))  { if (v < 1.5f || v > 7.5f)   return fail("1.5..7.5 g (IMU range is 8 g)");
    Motion.crashG = v;  prefs.putFloat("crashG", v);  return true; }

  // --- voltages (ladder-checked together) ---
  float on = Engine.vRunOn, off = Engine.vRunOff, warn = Engine.chargeWarn, clear = Engine.chargeClear;
  const char* canon = nullptr;
  if      (!strcasecmp(k, "vRunOn"))      { if (v < 12.0f || v > 15.0f)  return fail("12..15 V");   on = v;    canon = "vRunOn"; }
  else if (!strcasecmp(k, "vRunOff"))     { if (v < 11.5f || v > 14.5f)  return fail("11.5..14.5 V"); off = v; canon = "vRunOff"; }
  else if (!strcasecmp(k, "chargeWarn"))  { if (v < 12.0f || v > 15.0f)  return fail("12..15 V");   warn = v;  canon = "chargeWarn"; }
  else if (!strcasecmp(k, "chargeClear")) { if (v < 12.0f || v > 15.5f)  return fail("12..15.5 V"); clear = v; canon = "chargeClear"; }
  else return fail("unknown key");

  if (!ladderOk(off, on, warn, clear)) return fail("must keep vRunOff < vRunOn < chargeWarn < chargeClear");

  Engine.vRunOn = on; Engine.vRunOff = off; Engine.chargeWarn = warn; Engine.chargeClear = clear;
  prefs.putFloat(canon, v);
  return true;
}

bool Config::getValue(const char* k, float& o) const {
  if      (!strcasecmp(k, "vRunOn"))         o = Engine.vRunOn;
  else if (!strcasecmp(k, "vRunOff"))        o = Engine.vRunOff;
  else if (!strcasecmp(k, "chargeWarn"))     o = Engine.chargeWarn;
  else if (!strcasecmp(k, "chargeClear"))    o = Engine.chargeClear;
  else if (!strcasecmp(k, "runDebounce"))    o = Engine.runDebounceMs;
  else if (!strcasecmp(k, "stallDebounce"))  o = Engine.stallDebounceMs;
  else if (!strcasecmp(k, "offDelay"))       o = Engine.offDelayMs;
  else if (!strcasecmp(k, "backstop"))       o = Engine.backstopMs;
  else if (!strcasecmp(k, "chargeDebounce")) o = Engine.chargeDebounceMs;
  else if (!strcasecmp(k, "ignDebounce"))    o = Engine.ignDebounceMs;
  else if (!strcasecmp(k, "shedStep"))       o = Loads.shedStepMs;
  else if (!strcasecmp(k, "crankHold"))      o = Loads.crankHoldMs;
  else if (!strcasecmp(k, "moveDeg"))        o = Motion.moveDeg;
  else if (!strcasecmp(k, "tipDeg"))         o = Motion.tipDeg;
  else if (!strcasecmp(k, "crashG"))         o = Motion.crashG;
  else if (!strcasecmp(k, "crashCancel"))    o = Motion.cancelWindowMs;
  else return false;
  return true;
}

void Config::resetDefaults() {
  applyDefaults();
  persistAll();
  LOGI("config: reset to firmware defaults");
}
