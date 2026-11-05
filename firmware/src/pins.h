// ===== pins.h — ESP32 GPIO map =====
// The single place that maps logical signals to physical ESP32 pins.
// Rationale and constraints are recorded in docs/adr/0009-gpio-pin-map.md.
//
// Relay enables are active-HIGH into the ULN2803A inputs. Every pin chosen
// here sits LOW at boot, so all relays stay OFF while the ESP boots — which
// matches the 10 kΩ pull-downs on the ULN inputs and the accessory fail-OFF rule.
//
// One exception to "no strapping pins" (ADR-0018): AUX_EN is GPIO 12 (MTDI), which
// selects the flash voltage at reset. LOW = 3.3 V flash, the right level for the
// WROOM-32D, and also relay-OFF; its 10 kΩ pull-down (R23) holds both. Never drive
// it HIGH before setup(), and never fit a pull-up on it.
#pragma once

#include <Arduino.h>

// --- Relay coil enables (ULN2803A inputs 1..5) ---
constexpr uint8_t PIN_MASTER_EN = 13;   // ULN ch1 -> J2: external master relay (gates the whole box)
constexpr uint8_t PIN_FOG_EN    = 25;   // ULN ch2 -> J3.1: box relay, fog lights
constexpr uint8_t PIN_GRIP_EN   = 26;   // ULN ch3 -> J3.2: box relay, heated grips
constexpr uint8_t PIN_SPARE_EN  = 27;   // ULN ch4 -> J3.3: box relay, spare load
constexpr uint8_t PIN_AUX_EN    = 12;   // ULN ch5 -> J3.4: box relay, aux load (strapping pin, see above)

// --- Inputs (opto-isolated 12 V sense; logic inverts: 12 V present -> LOW) ---
constexpr uint8_t PIN_IGN_SENSE   = 34; // ignition/run-sense; input-only + RTC (ext0 wake)
constexpr uint8_t PIN_START_SENSE = 35; // start-button / headlight-feed sense; input-only + RTC
constexpr uint8_t PIN_IMU_INT     = 33; // IMU wake-on-motion interrupt; RTC (deep-sleep wake)

// --- I2C bus (INA3221 bus-voltage sense + 6-axis IMU) ---
constexpr uint8_t PIN_I2C_SDA   = 21;
constexpr uint8_t PIN_I2C_SCL   = 22;

// --- Aux buttons (ADR-0014, ADR-0016): dry contacts, active-LOW with internal pull-up ---
// BTN1..BTN8; 7 used by the handlebar case + 1 spare. All pull-up-capable, no
// strapping/flash pins. In ring mode BTN5 is pulled low as a scan line, never driven
// high. Past 8 buttons -> MCP23017 on I2C (board rev, no re-wire).
constexpr uint8_t PIN_BTN[] = { 4, 14, 16, 17, 18, 19, 23, 32 };

// --- Onboard LED (heartbeat / bench relay-activity indicator) ---
constexpr uint8_t PIN_LED       = 2;
