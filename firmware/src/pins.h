// ===== pins.h — ESP32 GPIO map =====
// The single place that maps logical signals to physical ESP32 pins.
// Rationale and constraints are recorded in docs/adr/0009-gpio-pin-map.md.
//
// Relay enables are active-HIGH into the ULN2803A inputs. Every pin chosen
// here sits LOW at boot (none are strapping or flash pins), so all relays
// stay OFF while the ESP boots — which matches the 10 kΩ pull-downs on the
// ULN inputs and the accessory fail-OFF rule.
#pragma once

#include <Arduino.h>

// --- Relay coil enables (ULN2803A inputs 1..4) ---
constexpr uint8_t PIN_MASTER_EN = 13;   // external master relay (gates the whole box)
constexpr uint8_t PIN_FOG_EN    = 25;   // fog lights
constexpr uint8_t PIN_GRIP_EN   = 26;   // heated grips (future load)
constexpr uint8_t PIN_SPARE_EN  = 27;   // reserved aux channel

// --- Inputs (opto-isolated 12 V sense; logic inverts: 12 V present -> LOW) ---
constexpr uint8_t PIN_IGN_SENSE   = 34; // ignition/run-sense; input-only + RTC (ext0 wake)
constexpr uint8_t PIN_START_SENSE = 35; // start-button / headlight-feed sense; input-only + RTC
constexpr uint8_t PIN_IMU_INT     = 33; // IMU wake-on-motion interrupt; RTC (deep-sleep wake)

// --- I2C bus (INA3221 bus-voltage sense + 6-axis IMU) ---
constexpr uint8_t PIN_I2C_SDA   = 21;
constexpr uint8_t PIN_I2C_SCL   = 22;

// --- Onboard LED (heartbeat / bench relay-activity indicator) ---
constexpr uint8_t PIN_LED       = 2;
