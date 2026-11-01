// Minimal Arduino shim for native (host) unit tests — just enough for the
// supervisor/sensors/relays to compile and run under g++. Time is a fake clock
// the test drives directly, so debounces/timers resolve instantly.
#pragma once
#include <stdint.h>
#include <stddef.h>

extern unsigned long _hostMillis;                 // defined in the test
static inline unsigned long millis() { return _hostMillis; }

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

#ifdef MOCK_PINS
extern uint8_t _hostMode[40];                     // defined in the test
extern uint8_t _hostLatch[40];
int _hostRead(uint8_t p);                         // the test models the wiring
static inline void pinMode(uint8_t p, uint8_t m) { _hostMode[p] = m; }
static inline void digitalWrite(uint8_t p, uint8_t v) { _hostLatch[p] = v; }
static inline int  digitalRead(uint8_t p) { return _hostRead(p); }
#else
static inline void pinMode(uint8_t, uint8_t) {}
static inline void digitalWrite(uint8_t, uint8_t) {}
static inline int  digitalRead(uint8_t) { return LOW; }
#endif
