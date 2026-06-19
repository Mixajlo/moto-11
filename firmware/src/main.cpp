// ===== V-Strom Smart Cockpit — firmware skeleton =====
// Milestone 1: connect to WiFi, advertise for OTA, blink a heartbeat.
// Same code on both ESPs; DEVICE_NAME comes from the build environment.

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "secrets.h"

#ifndef DEVICE_NAME
#define DEVICE_NAME "vstrom-esp"   // fallback if no build flag is set
#endif
#include <TelnetStream.h>

const uint8_t LED_PIN = 2;         // onboard LED on most ESP32 dev boards
uint32_t lastBlink = 0;
bool ledOn = false;

void setupOTA() {
  ArduinoOTA.setHostname(DEVICE_NAME);
  ArduinoOTA.setPassword(OTA_PASS);

  ArduinoOTA.onStart([]() { Serial.println("\n[OTA] update starting"); });
  ArduinoOTA.onEnd([]()   { Serial.println("\n[OTA] done — rebooting");  });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    Serial.printf("[OTA] %u%%\r", (p * 100) / t);
  });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("\n[OTA] error %u\n", e);
  });
  ArduinoOTA.begin();
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_NAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting to \"%s\" ", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.printf("\nWiFi OK   IP %s   host %s.local\n",
                WiFi.localIP().toString().c_str(), DEVICE_NAME);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  delay(300);
  Serial.printf("\n\n=== %s booting ===\n", DEVICE_NAME);

  connectWiFi();
  TelnetStream.begin();
  setupOTA();
  Serial.println("OTA ready. Heartbeat running.");
}

void loop() {
  ArduinoOTA.handle();   // must be called continuously to receive OTA pushes

  // 1 Hz heartbeat = proof of life.
  // To PROVE OTA later: change 1000 to 200 here, then OTA-upload and watch
  // the LED start blinking fast without ever touching the USB cable.
  if (millis() - lastBlink >= 1200) {
    lastBlink = millis();
    ledOn = !ledOn;
    digitalWrite(LED_PIN, ledOn);
    Serial.printf("[hb] %s\n", ledOn ? "on" : "off");
    TelnetStream.printf("[hb] %s\n", ledOn ? "on" : "off");
  }
}