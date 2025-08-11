#ifndef ZONES_H
#define ZONES_H
#include <EEPROM.h>
#include <ArduinoJson.h>
#define ZONE_DESC_MAX_LEN 20
enum ZoneState { ZONE_CLEAR, ZONE_TRIGGERED, ZONE_BYPASSED };
extern char zoneDesc[TOTAL_ZONES][ZONE_DESC_MAX_LEN + 1];
extern bool zoneBypassed[TOTAL_ZONES];
extern ZoneState zoneStates[TOTAL_ZONES];
extern bool testMode;
void setupZones();
void loadZoneSettings();
void saveZoneSettings();
bool bypassClear();
bool zoneTriggered(int zone);
void updateZoneState(int zone, ZoneState newState);
String buildZonesJSON();
String buildZoneStatusJSON();
void publishAllZonesMQTT();
void publishLastTriggeredZone(int zone);
#endif