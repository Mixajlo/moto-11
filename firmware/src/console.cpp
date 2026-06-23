// ===== console.cpp =====
#include <Arduino.h>
#include <TelnetStream.h>
#include <string.h>

#include "console.h"
#include "relays.h"
#include "sensors.h"
#include "supervisor.h"

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
  outln("  state | sup        show engine-run supervisor state");
  outln("  sim ign on|off     (bench) fake the ignition input");
  outln("  sim vbus <volts>   (bench) fake the bus voltage, e.g. sim vbus 13.4");
  outln("  sim real | sim     switch to real sensors / back to sim");
  outln("  help | ?           this list");
  outln("  channels: master  fog  grip  spare");
  outln("(onboard LED: heartbeat when all off, solid ON when any relay is on)");
}

void printSupState() {
  char line[120];
  snprintf(line, sizeof(line),
           "state=%s  ign=%s  vbus=%.2f  sim=%s  master=%s",
           Engine.stateName(),
           Sensor.ignition() ? "ON" : "off",
           Sensor.busVoltage(),
           Sensor.sim() ? "yes" : "no",
           Relays.isOn(RELAY_MASTER) ? "ON" : "off");
  outln(line);
}

// Handle "sim ..." subcommands. `a` = first arg, `b` = second (may be null).
void handleSim(const char* a, const char* b) {
  if (a && !strcasecmp(a, "ign") && b) {
    bool on = !strcasecmp(b, "on") || !strcmp(b, "1");
    Sensor.setSimIgn(on);
    out("sim ign -> "); outln(on ? "ON" : "off");
  } else if (a && !strcasecmp(a, "vbus") && b) {
    Sensor.setSimVbus(atof(b));
    printSupState();
  } else if (a && !strcasecmp(a, "real")) {
    Sensor.setSim(false); outln("sensors: real");
  } else if (a && !strcasecmp(a, "sim")) {
    Sensor.setSim(true);  outln("sensors: sim");
  } else {
    outln("usage: sim ign on|off  |  sim vbus <volts>  |  sim real|sim");
  }
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
  } else if (!strcasecmp(cmd, "state") || !strcasecmp(cmd, "sup")) {
    printSupState();
  } else if (!strcasecmp(cmd, "sim")) {
    handleSim(arg, arg2);
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
