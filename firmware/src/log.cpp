// ===== log.cpp =====
#include "log.h"

#include <Arduino.h>
#include <TelnetStream.h>
#include <stdarg.h>
#include <stdio.h>

void logLine(const char* level, const char* fmt, ...) {
  char msg[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);

  char line[200];
  snprintf(line, sizeof(line), "[%lu][%s] %s",
           static_cast<unsigned long>(millis()), level, msg);

  Serial.println(line);
  TelnetStream.println(line);   // no-op until a telnet client is connected
}
