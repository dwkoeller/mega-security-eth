# Mega Security System Firmware

This firmware is for Arduino Mega 2560 with Ethernet Shield.
It includes:
- Buzzer countdowns (exit + entry delay, urgent last 5 sec)
- EEPROM-backed zone descriptions & bypass flags
- MQTT integration with alarm state, countdown, per-zone states, master JSON, last triggered zone
- Password-protected /zones web UI (light/dark theme, Save All, Test Mode, Reboot)
- Home Assistant integration config included
