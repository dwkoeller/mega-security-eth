#include "credentials.h"
#include "Zones.h"
#include "MQTTHandler.h"
#include "Countdown.h"
#include "WebEndpoints.h"
#include "AlarmStates.h"

void setup() {
  Serial.begin(115200);
  Serial.println(F("Mega Security System Starting..."));

  setupZones();
  setupMQTT();
  setupCountdown();
  setupWebEndpoints();
}

void loop() {
  loopZones();
  loopMQTT();
  loopCountdown();
  loopWebEndpoints();
}