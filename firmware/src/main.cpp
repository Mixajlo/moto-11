// ===== V-Strom Smart Cockpit — firmware =====
// Milestone 3: autonomous engine-run supervisor (SLEEP/ARMED/RUNNING/OFF_DELAY)
// on top of the relay control.
//   - RelayController owns the four ULN2803-driven coil enables (fail-OFF).
//   - Sensors provides bus voltage + ignition (bench SIM until INA3221/opto wired).
//   - Supervisor drives the MASTER relay off the engine-run signal.
//   - Serial/Telnet console drives relays AND injects fake sim inputs for bench.
//   - WiFi + ArduinoOTA + heartbeat carry over from earlier milestones.
// Same code on both ESPs; DEVICE_NAME comes from the build environment.

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <TelnetStream.h>

#include "secrets.h"
#include "pins.h"
#include "relays.h"
#include "console.h"
#include "sensors.h"
#include "supervisor.h"
#include "config.h"
#include "buttons.h"

#ifndef DEVICE_NAME
#define DEVICE_NAME "vstrom-esp"   // fallback if no build flag is set
#endif

// The live firmware (setup/loop + WiFi/OTA) is excluded from unit-test builds —
// `pio test` provides its own runner. PIO_UNIT_TESTING is defined during tests.
#ifndef PIO_UNIT_TESTING

uint32_t lastBlink  = 0;
bool      ledOn      = false;
bool      netUp      = false;   // true once WiFi+OTA+telnet are live
uint32_t  lastWifiTry = 0;

void setupOTA() {
  ArduinoOTA.setHostname(DEVICE_NAME);
  ArduinoOTA.setPassword(OTA_PASS);

  // Force relays OFF before a flash so a half-written image can't leave a coil
  // energised, and so the box is in a known state on reboot.
  ArduinoOTA.onStart([]() { Relays.allOff(); Serial.println("\n[OTA] update starting"); });
  ArduinoOTA.onEnd([]()   { Serial.println("\n[OTA] done — rebooting"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    Serial.printf("[OTA] %u%%\r", (p * 100) / t);
  });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("\n[OTA] error %u\n", e);
  });
  ArduinoOTA.begin();
}

// Bring up the services that need an IP. Called once WiFi first associates.
void startNetServices() {
  TelnetStream.begin();
  setupOTA();
  netUp = true;
  Serial.printf("WiFi OK   IP %s   host %s.local — OTA + telnet console live\n",
                WiFi.localIP().toString().c_str(), DEVICE_NAME);
}

void setup() {
  Serial.begin(115200);

  // Relays first: get every coil enable driven LOW before anything else can
  // block. Accessories are fail-OFF, so this is the safest possible boot state.
  Relays.begin();
  Sensor.begin();
  Engine.begin();
  Cfg.begin();      // overlay NVS-saved tunables onto the supervisor defaults
  Btns.begin();     // handlebar aux buttons (INPUT_PULLUP, fail to "not pressed")

  pinMode(PIN_LED, OUTPUT);
  delay(300);
  Serial.printf("\n\n=== %s booting ===\n", DEVICE_NAME);

  // Non-blocking: kick off the association and move on. The relay console runs
  // over USB serial whether or not WiFi ever comes up — essential on the bench.
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_NAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("WiFi: connecting to \"%s\" in the background...\n", WIFI_SSID);

  consoleBegin();
  Serial.println("Bench relay console live on USB serial. Type 'help'.");
}

void loop() {
  // Bring up / keep up the network without ever blocking the relay console.
  if (WiFi.status() == WL_CONNECTED) {
    if (!netUp) startNetServices();
    ArduinoOTA.handle();   // must run continuously to receive OTA pushes
  } else if (millis() - lastWifiTry >= 5000) {
    lastWifiTry = millis();
    netUp = false;
    WiFi.reconnect();
  }

  consolePoll(netUp);    // read + dispatch any typed commands (USB always; telnet once netUp)
  Btns.update();         // debounce handlebar buttons; a press toggles its mapped relay
  Engine.update();       // run the engine-run state machine (drives the MASTER relay)

  // Onboard LED doubles as a bench indicator: heartbeat (proof of life) while
  // every relay is off; solid ON the moment any channel is energised — so you
  // get visible feedback even before the ULN/relay box is wired.
  if (Relays.anyOn()) {
    digitalWrite(PIN_LED, HIGH);
  } else if (millis() - lastBlink >= 1200) {
    lastBlink = millis();
    ledOn = !ledOn;
    digitalWrite(PIN_LED, ledOn);
  }
}

#endif  // PIO_UNIT_TESTING
