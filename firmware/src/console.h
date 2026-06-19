// ===== console.h — bench command console =====
// A tiny line-oriented command interpreter for driving the relays by hand on
// the bench, over USB serial AND Telnet (so you can poke the device wirelessly
// once it's on WiFi). This is bench/diagnostic plumbing — the autonomous engine
// -run state machine (ARMED/RUNNING/OFF_DELAY) lands later, on top of the same
// RelayController, once the INA226 + ignition-sense front-end is wired.
#pragma once

void consoleBegin();              // print the banner/help once at boot
void consolePoll(bool telnetUp);  // call every loop(); reads Serial always, Telnet when up
