// ===== console.cpp =====
#include <Arduino.h>
#include <TelnetStream.h>
#include <Wire.h>
#include <string.h>
#include <math.h>

#include "console.h"
#include "relays.h"
#include "sensors.h"
#include "supervisor.h"
#include "config.h"
#include "buttons.h"
#include "loads.h"
#include "pins.h"

namespace {

// One line buffer per input source so a half-typed serial line and a
// half-typed telnet line don't clobber each other.
char    serialBuf[64];
uint8_t serialLen = 0;
char    telnetBuf[64];
uint8_t telnetLen = 0;

// Echo to both consoles, so the result shows up wherever you typed.
void out(const char* s)   { Serial.print(s);   TelnetStream.print(s);   }
void outln(const char* s) { Serial.println(s);  TelnetStream.println(s); }
void outln()              { Serial.println();   TelnetStream.println();  }

void printHelp() {
  outln();
  outln("=== moto-11 bench relay console ===");
  outln("  status | s        show every channel + GPIO + state");
  outln("  on   <ch|all>      energise a channel (relay pulls in)");
  outln("  off  <ch|all>      de-energise a channel");
  outln("  toggle <ch>        flip a channel");
  outln("  selftest           click each relay in turn (300ms each)");
  outln("  state | sup        show engine-run state (+ countdown to next timed change)");
  outln("  timer | t          time left until the next timed state change");
  outln("  sim ign on|off     (bench) fake the ignition input");
  outln("  sim start on|off   (bench) fake the start button (cranking)");
  outln("  sim vbus <volts>   (bench) fake the bus voltage, e.g. sim vbus 13.4");
  outln("  loads              load policy: wanted vs allowed, shed level, why held off");
  outln("  loads want <ch> <on|off>  ask for a load like a button would (grip waits for the engine)");
  outln("  sim real | sim     switch to real sensors / back to sim");
  outln("  sensors | sense    raw INA3221 CH1-3 + IGN/START opto pins (sensors probe = re-detect)");
  outln("  i2c                scan the I2C bus");
  outln("  imu                one IMU reading: accel, tilt, gyro, temp (imu probe = re-detect)");
  outln("  config             list tunables (config reset = restore defaults)");
  outln("  get <key>          show one tunable");
  outln("  set <key> <val>    change + persist a tunable (validated)");
  outln("  btn                show button states + mapping");
  outln("  btn press <1-8>    simulate a button press (bench)");
  outln("  btn map <1-8> <fog|grip|spare|aux|none> [push|latch]");
  outln("                     change + persist what a button does");
  outln("  btn reset          restore the default button mapping");
  outln("  btn ring <on|off>  on = factory loop wiring, off = one wire per button");
  outln("  help | ?           this list");
  outln("  channels: master  fog  grip  spare  aux");
  outln("(onboard LED: heartbeat when all off, solid ON when any relay is on)");
}

// Format a millisecond span as "9m41s" or "41s".
void fmtDuration(uint32_t ms, char* buf, size_t n) {
  uint32_t s = ms / 1000;
  if (s >= 60) snprintf(buf, n, "%lum%02lus", (unsigned long)(s / 60), (unsigned long)(s % 60));
  else         snprintf(buf, n, "%lus", (unsigned long)s);
}

void printSupState() {
  char line[140];
  // charge= is only meaningful while RUNNING (charge health isn't judged otherwise).
  const char* charge = Engine.state() != SUP_RUNNING ? "-"
                     : Engine.chargeMarginal() ? "MARGINAL" : "ok";
  snprintf(line, sizeof(line),
           "state=%s  ign=%s  vbus=%.2f  sim=%s  master=%s  charge=%s",
           Engine.stateName(),
           Sensor.ignition() ? "ON" : "off",
           Sensor.busVoltage(),
           Sensor.sim() ? "yes" : "no",
           Relays.isOn(RELAY_MASTER) ? "ON" : "off",
           charge);
  outln(line);

  const char* what = nullptr;
  uint32_t left = Engine.timeLeftMs(&what);
  if (what) {
    char d[16]; fmtDuration(left, d, sizeof(d));
    char t[140]; snprintf(t, sizeof(t), "  timer: %s in %s", what, d);
    outln(t);
  } else {
    outln("  timer: none in this state");
  }
}

// Handle "sim ..." subcommands. `a` = first arg, `b` = second (may be null).
void handleSim(const char* a, const char* b) {
  if (a && !strcasecmp(a, "ign") && b) {
    bool on = !strcasecmp(b, "on") || !strcmp(b, "1");
    Sensor.setSimIgn(on);
    out("sim ign -> "); outln(on ? "ON" : "off");
  } else if (a && !strcasecmp(a, "start") && b) {
    bool on = !strcasecmp(b, "on") || !strcmp(b, "1");
    Sensor.setSimStart(on);
    out("sim start -> "); outln(on ? "HELD (cranking)" : "released");
  } else if (a && !strcasecmp(a, "vbus") && b) {
    Sensor.setSimVbus(atof(b));
    printSupState();
  } else if (a && !strcasecmp(a, "real")) {
    Sensor.setSim(false); outln("sensors: real");
  } else if (a && !strcasecmp(a, "sim")) {
    Sensor.setSim(true);  outln("sensors: sim");
  } else {
    outln("usage: sim ign on|off  |  sim start on|off  |  sim vbus <volts>  |  sim real|sim");
  }
}

// One IMU snapshot: accel (g) + its magnitude, tilt from gravity, gyro, die temp.
// At rest the magnitude reads ~1.00 g whatever the orientation.
void printImu() {
  outln();
  if (!Sensor.imuPresent()) {
    outln("IMU: NOT found (VCC 3.3 V, GND, SDA=21, SCL=22; 'i2c' to scan, 'imu probe' to retry)");
    return;
  }
  ImuSample s;
  if (!Sensor.imuRead(s)) { outln("IMU: read failed (I2C error)"); return; }
  char l[120];
  snprintf(l, sizeof(l), "IMU @0x%02X (WHO_AM_I 0x%02X)", Sensor.imuAddress(), Sensor.imuWhoAmI());
  outln(l);
  float mag = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
  snprintf(l, sizeof(l), "  accel  x %+6.3f  y %+6.3f  z %+6.3f g   |a| %.3f g", s.ax, s.ay, s.az, mag);
  outln(l);
  float roll  = atan2f(s.ay, s.az) * 57.2958f;
  float pitch = atan2f(-s.ax, sqrtf(s.ay * s.ay + s.az * s.az)) * 57.2958f;
  snprintf(l, sizeof(l), "  tilt   roll %+6.1f deg   pitch %+6.1f deg  (from gravity)", roll, pitch);
  outln(l);
  snprintf(l, sizeof(l), "  gyro   x %+7.2f  y %+7.2f  z %+7.2f deg/s", s.gx, s.gy, s.gz);
  outln(l);
  snprintf(l, sizeof(l), "  temp   %.1f C (die)", s.tempC);
  outln(l);
}

// Load policy view (ADR-0019): what the rider wants vs what is allowed, and why not.
void printLoads() {
  outln();
  char l[120];
  snprintf(l, sizeof(l), "loads: shed level %u (0 none, 1 grip, 2 grip+fog)  crank hold %s",
           (unsigned)Loads.shedLevel(), Loads.crankHold() ? "ACTIVE" : "no");
  outln(l);
  outln("  load   want  relay  held off because");
  for (RelayId r : { RELAY_GRIP, RELAY_FOG }) {
    uint8_t why = Loads.blockedWhy(r);
    char reasons[64] = "";
    if (why & WHY_ENGINE_OFF) strcat(reasons, "engine-not-running ");
    if (why & WHY_CHARGE)     strcat(reasons, "charging-marginal ");
    if (why & WHY_CRANK)      strcat(reasons, "cranking ");
    snprintf(l, sizeof(l), "  %-5s  %-4s  %-5s  %s", Relays.name(r),
             Loads.wanted(r) ? "yes" : "no", Relays.isOn(r) ? "ON" : "off",
             why ? reasons : "-");
    outln(l);
  }
  outln("  (console on/off bypasses this; buttons and 'loads want' go through it)");
}

void printConfig() {
  outln();
  outln("config (set <key> <val> | config reset):");
  for (int i = 0; i < Config::KEY_COUNT; i++) {
    const char* k = Config::key(i);
    float v = 0;
    Cfg.getValue(k, v);
    char line[48];
    snprintf(line, sizeof(line), "  %-15s %g", k, v);
    outln(line);
  }
}

void handleSet(const char* key, const char* val) {
  const char* err = nullptr;
  if (Cfg.set(key, atof(val), &err)) {
    float v = 0; Cfg.getValue(key, v);
    char line[48];
    snprintf(line, sizeof(line), "%s = %g (saved)", key, v);
    outln(line);
  } else {
    out("rejected: "); outln(err ? err : "?");
  }
}

void printButtons() {
  outln();
  outln("buttons (btn press <1-8> to simulate, btn map to change):");
  out("  ring scan: "); outln(Btns.ring() ? "on (BTN4-7 read through the loop)" : "off");
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    RelayId r = Btns.mapping(i);
    char line[64];
    char source[8];
    if (Btns.inRing(i)) snprintf(source, sizeof(source), "ring");
    else                snprintf(source, sizeof(source), "gpio%u", PIN_BTN[i]);
    snprintf(line, sizeof(line), "  BTN%u  %-6s  %-7s  %-5s -> %s",
             i + 1, source,
             Btns.pressed(i) ? "PRESSED" : "-",
             Btns.latching(i) ? "latch" : "push",
             r < RELAY_COUNT ? Relays.name(r) : "(unmapped)");
    outln(line);
  }
}

// btn map <1-8> <fog|grip|spare|aux|none> [push|latch]; the type defaults to the current one.
void handleBtnMap(const char* num, const char* target, const char* type) {
  int n = num ? atoi(num) : 0;
  if (n < 1 || n > BUTTON_COUNT || !target) {
    outln("btn map <1-8> <fog|grip|spare|aux|none> [push|latch]");
    return;
  }
  uint8_t i = static_cast<uint8_t>(n - 1);

  RelayId relay = RELAY_MASTER;              // refused by configure() unless a name matches
  if (!strcasecmp(target, "none")) relay = RELAY_COUNT;
  else if (strcasecmp(target, "master")) {
    bool found = false;
    for (uint8_t r = RELAY_FOG; r < RELAY_COUNT; r++) {
      if (!strcasecmp(target, Relays.name(static_cast<RelayId>(r)))) {
        relay = static_cast<RelayId>(r);
        found = true;
      }
    }
    if (!found) { out("rejected: unknown relay "); outln(target); return; }
  }

  bool latching = Btns.latching(i);
  if (type) {
    if      (!strcasecmp(type, "latch")) latching = true;
    else if (!strcasecmp(type, "push"))  latching = false;
    else { out("rejected: unknown type "); outln(type); return; }
  }

  const char* err = nullptr;
  if (Btns.configure(i, relay, latching, &err)) printButtons();
  else { out("rejected: "); outln(err ? err : "?"); }
}

void printStatus() {
  outln();
  outln("ch       gpio  state");
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    char line[40];
    snprintf(line, sizeof(line), "%-8s %4u  %s",
             Relays.name(static_cast<RelayId>(i)),
             Relays.pin(static_cast<RelayId>(i)),
             Relays.isOn(static_cast<RelayId>(i)) ? "ON" : "off");
    outln(line);
  }
}

// Returns RelayId index, or -1 if the token isn't a known channel name.
int findChannel(const char* tok) {
  for (uint8_t i = 0; i < RELAY_COUNT; i++)
    if (strcasecmp(tok, Relays.name(static_cast<RelayId>(i))) == 0) return i;
  return -1;
}

void setOne(RelayId id, bool on) {
  Relays.set(id, on);
  char line[40];
  snprintf(line, sizeof(line), "%s -> %s", Relays.name(id), on ? "ON" : "off");
  outln(line);
}

void setAllOrOne(const char* tok, bool on) {
  if (strcasecmp(tok, "all") == 0) {
    for (uint8_t i = 0; i < RELAY_COUNT; i++) Relays.set(static_cast<RelayId>(i), on);
    outln(on ? "all -> ON" : "all -> off");
    return;
  }
  int id = findChannel(tok);
  if (id < 0) { out("unknown channel: "); outln(tok); return; }
  setOne(static_cast<RelayId>(id), on);
}

// Blocking on purpose: a manual bench check is not running concurrently with an
// OTA push, and the short delays keep the click cadence audible/measurable.
void selftest() {
  outln("selftest: clicking each relay (300ms on, 300ms gap)...");
  Relays.allOff();
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    RelayId id = static_cast<RelayId>(i);
    char line[40];
    snprintf(line, sizeof(line), "  %s ON", Relays.name(id));
    outln(line);
    Relays.set(id, true);
    delay(300);
    Relays.set(id, false);
    delay(300);
  }
  outln("selftest done. all off.");
}

// Raw hardware readout, independent of sim: what the INA3221 and the two opto
// inputs see right now. The tool for proving the sensing wiring on the bench.
void printSensors() {
  char line[96];
  outln();
  if (Sensor.inaPresent()) {
    snprintf(line, sizeof(line), "INA3221 @0x%02X: found", Sensor.inaAddress());
    outln(line);
    for (uint8_t ch = 1; ch <= 3; ch++) {
      float v = 0, mv = 0;
      if (Sensor.inaChannel(ch, v, mv))
        snprintf(line, sizeof(line), "  CH%u  bus %6.3f V   shunt %+8.2f mV%s",
                 ch, v, mv, ch == 1 ? "   <- battery sense (vbus)" : "");
      else
        snprintf(line, sizeof(line), "  CH%u  read error", ch);
      outln(line);
    }
  } else {
    outln("INA3221: NOT found (check VCC 3.3 V, GND, SDA=21, SCL=22; 'i2c' to scan, 'sensors probe' to retry)");
  }
  // Opto logic inverts: 12 V present -> transistor on -> pin pulled LOW.
  snprintf(line, sizeof(line), "  IGN   gpio%-2u  pin %-4s -> ignition %s",
           PIN_IGN_SENSE, Sensor.ignRawLow() ? "LOW" : "HIGH",
           Sensor.ignRawLow() ? "ON (12 V sensed)" : "off");
  outln(line);
  snprintf(line, sizeof(line), "  START gpio%-2u  pin %-4s -> start %s",
           PIN_START_SENSE, Sensor.startRawLow() ? "LOW" : "HIGH",
           Sensor.startRawLow() ? "PRESSED (12 V sensed)" : "released");
  outln(line);
  snprintf(line, sizeof(line), "  supervisor uses: %s  (vbus=%.2f ign=%s)",
           Sensor.sim() ? "SIM values ('sim real' to switch)" : "REAL sensors",
           Sensor.busVoltage(), Sensor.ignition() ? "ON" : "off");
  outln(line);
  // Same snapshot as the readings above, so a watcher never pairs an old input
  // with a newer state (two separate commands can straddle a switch flip).
  snprintf(line, sizeof(line), "  state=%s  master=%s",
           Engine.stateName(), Relays.isOn(RELAY_MASTER) ? "ON" : "off");
  outln(line);
}

void i2cScan() {
#if defined(ARDUINO)
  outln("I2C scan (SDA 21, SCL 22):");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      char line[48];
      snprintf(line, sizeof(line), "  0x%02X%s", a,
               (a >= 0x40 && a <= 0x43) ? "  (INA3221 range)"
               : (a == 0x68 || a == 0x69) ? "  (MPU-6050 IMU)" : "");
      outln(line);
      found++;
    }
  }
  if (!found) outln("  nothing answered - check power, GND, SDA/SCL not swapped, pull-ups");
#endif
}

void dispatch(char* line) {
  char* cmd = strtok(line, " \t");
  if (!cmd) return;
  char* arg  = strtok(nullptr, " \t");
  char* arg2 = strtok(nullptr, " \t");

  if (!strcasecmp(cmd, "help") || !strcmp(cmd, "?")) {
    printHelp();
  } else if (!strcasecmp(cmd, "status") || !strcasecmp(cmd, "s")) {
    printStatus();
  } else if (!strcasecmp(cmd, "on") && arg) {
    setAllOrOne(arg, true);
  } else if (!strcasecmp(cmd, "off") && arg) {
    setAllOrOne(arg, false);
  } else if (!strcasecmp(cmd, "toggle") && arg) {
    int id = findChannel(arg);
    if (id < 0) { out("unknown channel: "); outln(arg); }
    else setOne(static_cast<RelayId>(id), !Relays.isOn(static_cast<RelayId>(id)));
  } else if (!strcasecmp(cmd, "selftest")) {
    selftest();
  } else if (!strcasecmp(cmd, "state") || !strcasecmp(cmd, "sup") ||
             !strcasecmp(cmd, "timer") || !strcasecmp(cmd, "t")) {
    printSupState();
  } else if (!strcasecmp(cmd, "sim")) {
    handleSim(arg, arg2);
  } else if (!strcasecmp(cmd, "sensors") || !strcasecmp(cmd, "sense")) {
    if (arg && !strcasecmp(arg, "probe")) {
      outln(Sensor.probeIna() ? "INA3221 found" : "INA3221 not found");
    }
    printSensors();
  } else if (!strcasecmp(cmd, "imu")) {
    if (arg && !strcasecmp(arg, "probe")) outln(Sensor.probeImu() ? "IMU found" : "IMU not found");
    printImu();
  } else if (!strcasecmp(cmd, "i2c")) {
    i2cScan();
  } else if (!strcasecmp(cmd, "config")) {
    if (arg && !strcasecmp(arg, "reset")) { Cfg.resetDefaults(); outln("config reset to defaults"); }
    else printConfig();
  } else if (!strcasecmp(cmd, "get") && arg) {
    float v = 0;
    if (Cfg.getValue(arg, v)) { char l[48]; snprintf(l, sizeof(l), "%s = %g", arg, v); outln(l); }
    else { out("unknown key: "); outln(arg); }
  } else if (!strcasecmp(cmd, "set") && arg && arg2) {
    handleSet(arg, arg2);
  } else if (!strcasecmp(cmd, "loads")) {
    if (arg && !strcasecmp(arg, "want") && arg2) {
      int id = findChannel(arg2);
      char* v = strtok(nullptr, " \t");
      if (id <= RELAY_MASTER || !v) outln("loads want <fog|grip|spare|aux> <on|off>");
      else Loads.request(static_cast<RelayId>(id), !strcasecmp(v, "on") || !strcmp(v, "1"));
    }
    printLoads();
  } else if (!strcasecmp(cmd, "btn")) {
    if (arg && !strcasecmp(arg, "press") && arg2) {
      int n = atoi(arg2);
      if (n >= 1 && n <= BUTTON_COUNT) Btns.fire(static_cast<uint8_t>(n - 1));
      else outln("btn press <1-8>");
    } else if (arg && !strcasecmp(arg, "map")) {
      char* target = strtok(nullptr, " \t");
      handleBtnMap(arg2, target, strtok(nullptr, " \t"));
    } else if (arg && !strcasecmp(arg, "reset")) {
      Btns.resetMapping();
      printButtons();
    } else if (arg && !strcasecmp(arg, "ring")) {
      if (arg2 && !strcasecmp(arg2, "on"))       { Btns.setRing(true);  printButtons(); }
      else if (arg2 && !strcasecmp(arg2, "off")) { Btns.setRing(false); printButtons(); }
      else outln("btn ring <on|off>");
    } else printButtons();
  } else {
    out("? unknown command: "); outln(cmd);
    printHelp();
  }
}

// Pull all available bytes from one stream into its line buffer; dispatch on \n.
void feed(Stream& io, char* buf, uint8_t& len, uint8_t cap) {
  while (io.available()) {
    char c = static_cast<char>(io.read());
    if (c == '\r') continue;
    if (c == '\n') {
      buf[len] = '\0';
      if (len > 0) dispatch(buf);
      len = 0;
    } else if (len < cap - 1) {
      buf[len++] = c;
    } else {
      len = 0;  // overrun — drop the line rather than wrap garbage
    }
  }
}

}  // namespace

void consoleBegin() {
  printHelp();
}

void consolePoll(bool telnetUp) {
  feed(Serial, serialBuf, serialLen, sizeof(serialBuf));
  if (telnetUp)
    feed(TelnetStream, telnetBuf, telnetLen, sizeof(telnetBuf));
}
