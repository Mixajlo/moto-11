# 1. ESP32 as the authoritative controller; tablet as a thin BLE dashboard

- Status: Accepted
- Date: 2026-06-19
- Deciders: Miki

## Context
The cockpit needs a brain that owns all bike logic — relay control, sensing, security,
state machines, the off-delay and parked sentinel. A tablet is attractive as a display, but
it is a consumer device: it reboots, suspends apps, throttles on heat, and can be unplugged
or stolen. Putting safety- and function-critical logic on it would make the bike's behaviour
hostage to an app lifecycle.

## Decision
The **ESP32-WROOM-32D is the single source of truth** for all bike logic. It is always-hot,
deep-sleeps when parked, and wakes on ignition. The **tablet is a thin BLE display and an
Android Auto receiver — UI only, it never decides anything.** The **phone** handles
navigation and connectivity. Control flows ESP → (BLE) → tablet for display; the tablet sends
only user intents back.

## Consequences
- Bike function survives a tablet reboot, crash, removal, or theft — only the UI degrades.
- The ESP must be robust and self-sufficient (watchdog, sane defaults, deep-sleep budget).
- Adds a BLE link and a small GATT contract to design and maintain.
- Clean separation of concerns: hard real-time and safety on the ESP, presentation on the tablet.

## Alternatives considered
- **Tablet/Android-centric** (apps drive the bike): rejected — unreliable for anything
  safety-relevant, and hostage to Android's app lifecycle and thermal behaviour.
- **Raspberry Pi controller**: rejected — higher idle power, slow boot, SD-card reliability,
  and overkill for the control task.
