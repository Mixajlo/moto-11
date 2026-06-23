// ===== log.h — leveled logging, debug compiled out of production =====
// LOGI / LOGW / LOGE are always present (info / warning / error) — cheap and
// useful for field diagnostics over Telnet.
// LOGD (debug) is compiled to NOTHING unless MOTO_DEBUG is defined. The bench
// build defines it (see platformio.ini); the bike/production build does not, so
// debug chatter never ships.
//
// Each line is broadcast to Serial + Telnet as: [millis][LVL] message
#pragma once

// printf-style; the attribute lets the compiler check the format args.
void logLine(const char* level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

#define LOGI(...) logLine("I", __VA_ARGS__)
#define LOGW(...) logLine("W", __VA_ARGS__)
#define LOGE(...) logLine("E", __VA_ARGS__)

#ifdef MOTO_DEBUG
  #define LOGD(...) logLine("D", __VA_ARGS__)
#else
  #define LOGD(...) do {} while (0)   // vanishes in production; args not evaluated
#endif
