[README.md](https://github.com/user-attachments/files/21710174/README.md)
## Mega Security System Firmware

### Overview
This firmware runs on an **Arduino Mega 2560** with an **Ethernet Shield** to provide a fully functional home security system with **Home Assistant integration**, **MQTT control**, **web-based configuration**, and **physical alarm panel output**.

It merges traditional alarm panel behavior with smart home capabilities.

---

## Features
- **16 configurable zones** (expandable via `TOTAL_ZONES` in `credentials.h`).
- **EEPROM-backed** zone descriptions and bypass settings (persistent across reboots).
- **Buzzer countdowns**:
  - Exit delay when arming (1 beep/sec, urgent beeps last 5 seconds).
  - Entry delay before triggering alarm.
- **Alarm panel relay control** (active LOW).
- **MQTT integration** with:
  - Alarm state (`disarmed`, `armed_home`, `armed_away`, `armed_night`, `pending`, `triggered`)
  - Countdown timer
  - Per-zone topics with `triggered` / `bypassed` / `clear` states
  - Master JSON payload of all zones
  - Last triggered zone information
  - Test Mode status
- **Password-protected web UI**:
  - Light/dark theme toggle
  - Edit zone descriptions
  - Toggle bypass flags
  - Save all changes to EEPROM
  - Toggle Test Mode
  - Reboot controller
  - Live status color coding

---

## Hardware Setup
### Required Components
- Arduino Mega 2560
- Ethernet Shield (W5100/W5500)
- Magnetic door/window contacts or motion sensors
- Piezo buzzer (connected to `PIN_BUZZER` in `Countdown.cpp`)
- Relay module for alarm panel (connected to `PIN_ALARM` in `Zones.cpp`)

### Pin Assignments
Default for 16 zones:
```
Zone 1:  22    Zone 9:  30
Zone 2:  23    Zone 10: 31
Zone 3:  24    Zone 11: 32
Zone 4:  25    Zone 12: 33
Zone 5:  26    Zone 13: 34
Zone 6:  27    Zone 14: 35
Zone 7:  28    Zone 15: 36
Zone 8:  29    Zone 16: 37
```
- **Alarm relay output**: Pin 5 (`PIN_ALARM`)
- **Buzzer**: Pin 6 (`PIN_BUZZER`)

---

## Web Interface
### Accessing
1. Ensure Arduino is connected to your LAN and has an IP assigned (set in `credentials.h`).
2. Open browser and go to:
   ```
   http://<device-ip>/zones
   ```
3. Enter your credentials (`WEB_ADMIN_ID` / `WEB_ADMIN_PASSWORD` from `credentials.h`).

### Features
- **Zone Table**: Lists all zones with editable description and bypass checkbox.
- **Live Color Coding**:
  - Red = Triggered
  - Yellow = Bypassed
  - Normal = Clear
- **Save All**: Stores all changes to EEPROM.
- **Test Mode**: Disables alarm triggering but still logs zone activity.
- **Reboot**: Safely restarts the controller.
- **Theme Toggle**: Switch between light/dark mode (saved in browser).

### Data Persistence
All changes made in the web UI are written to EEPROM and remain after power loss.

---

## Home Assistant Integration
The file `home_assistant_config.yaml` contains ready-to-use MQTT entities.

### Alarm Control Panel
Allows arming/disarming from Home Assistant UI:
```yaml
alarm_control_panel:
  - platform: mqtt
    name: "Mega Security"
    state_topic: "home/alarm"
    command_topic: "home/alarm/set"
    payload_disarm: "disarm"
    payload_arm_home: "arm_home"
    payload_arm_away: "arm_away"
    payload_arm_night: "arm_night"
```

### Additional Entities
- **Countdown Sensor**: Shows remaining seconds in exit/entry delay.
- **Test Mode**: Binary sensor showing if system is in Test Mode.
- **Per-Zone Sensors**: Show triggered/bypassed/clear for each zone.
- **Last Triggered Zone**: Shows which zone caused last alarm.

---

## Alarm Panel Operation
- **Arming**:
  - Command from HA (`arm_home`, `arm_away`, `arm_night`) or web UI → system enters `pending` and starts **exit countdown**.
  - At countdown end, system arms if all non-bypassed zones are clear.
- **Triggering**:
  - Armed system detects a zone trip → **entry countdown** begins (unless in Test Mode).
  - If not disarmed before countdown ends → alarm relay activates (`PIN_ALARM` set LOW).
- **Disarming**:
  - Command from HA (`disarm`) or via direct function call in code → alarm relay deactivates, system returns to `disarmed`.

---

## MQTT Topics
| Topic | Payload | Description |
|-------|---------|-------------|
| `home/alarm` | `disarmed` / `armed_home` / `armed_away` / `armed_night` / `pending` / `triggered` | Current alarm state |
| `home/alarm/set` | `disarm` / `arm_home` / `arm_away` / `arm_night` | Command topic |
| `home/alarm/countdown` | seconds remaining | Countdown timer |
| `home/alarm/test_mode` | `on` / `off` | Test Mode status |
| `home/alarm/zones` | JSON object | All zone states |
| `home/alarm/zone/<n>` | `triggered` / `bypassed` / `clear` | Single zone state |
| `home/alarm/last_trigger` | JSON object | Zone number, description, and timestamp of last alarm trigger |

---

## EEPROM Settings
Each zone uses:
- **20 bytes**: ASCII description (padded with nulls).
- **1 byte**: Bypass flag (`0` = off, `1` = on`).

EEPROM layout:
```
Zone 1: [desc(20)][bypass(1)]
Zone 2: [desc(20)][bypass(1)]
...
```
