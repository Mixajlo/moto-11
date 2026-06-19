// ===== V-Strom Smart Cockpit — firmware =====
// Milestone 2: relay control on the bench.
//   - RelayController owns the four ULN2803-driven coil enables (fail-OFF).
//   - A serial/Telnet console drives them by hand for bench testing.
//   - WiFi + ArduinoOTA + heartbeat carry over from milestone 1.
// Same code on both ESPs; DEVICE_NAME comes from the build environment.
//
// Next: INA226 bus-voltage + ignition-sense front-end, then the autonomous
// engine-run supervisor (SLEEP/ARMED/RUNNING/OFF_DELAY) on top of these relays.

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <TelnetStream.h>

#include "secrets.h"
#include "pins.h"
#include "relays.h"
#include "console.h"

#ifndef DEVICE_NAME
#define DEVICE_NAME "vstrom-esp"   // fallback if no build flag is set
#endif

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
