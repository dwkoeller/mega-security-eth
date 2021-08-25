#include <uptime.h>

#include "credentials.h"

#define WEBDUINO_AUTH_REALM "Mega Security Authentication"
#define MQTT_SOCKET_TIMEOUT                  120

#include <Ethernet3.h>
#include <Dns.h>
#include <PubSubClient.h>
#include "Ticker.h"
#include <Time.h>
#include <SPI.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include "WebServer.h"
#include "base64.h"

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

//This can be used to output the date the code was compiled
const char compile_date[] = __DATE__ " " __TIME__;

#define WEB_ADMIN_ID                         "admin"
#define WEB_ADMIN_PASSWORD                   "password"
#define MQTT_DEVICE                          "mega-security" // Enter your MQTT device
#define MQTT_PORT                            1883 // Enter your MQTT server port.
#define FIRMWARE_VERSION                     "-2.01"
#define EEPROM_DATA_VERSION                  2
#define NTP_SERVER                           "pool.ntp.org"
#define MQTT_HEARTBEAT_SUB                   "heartbeat/#"
#define MQTT_HEARTBEAT_TOPIC                 "heartbeat"
#define MQTT_DISCOVERY_BINARY_SENSOR_PREFIX  "homeassistant/binary_sensor/"
#define MQTT_DISCOVERY_SENSOR_PREFIX         "homeassistant/sensor/"
#define MQTT_ALARM_COMMAND_TOPIC             "home/alarm/set"
#define MQTT_ALARM_STATE_TOPIC               "home/alarm"
#define HA_TELEMETRY                         "ha"

#define SS     10    //W5500 CS
#define RST    7    //W5500 RST For mega RST 11
#define CS  4     //SD CS pin

#define WATCHDOG   2
#define PIN_ALARM  5

#define PIN_ZONE1  22
#define PIN_ZONE2  23
#define PIN_ZONE3  24
#define PIN_ZONE4  25
#define PIN_ZONE5  26
#define PIN_ZONE6  27
#define PIN_ZONE7  28
#define PIN_ZONE8  29
#define PIN_ZONE9  30
#define PIN_ZONE10 31
#define PIN_ZONE11 32
#define PIN_ZONE12 33
#define PIN_ZONE13 34
#define PIN_ZONE14 35
#define PIN_ZONE15 36
#define PIN_ZONE16 37
#define PIN_ZONE17 38
#define PIN_ZONE18 39
#define PIN_ZONE19 40
#define PIN_ZONE20 41
#define PIN_ZONE21 42
#define PIN_ZONE22 43
#define PIN_ZONE23 44
#define PIN_ZONE24 45
#define PIN_ZONE25 46
#define PIN_ZONE26 47
#define PIN_ZONE27 48
#define PIN_ZONE28 49

#define ALARM_STATE_DISARMED    0
#define ALARM_STATE_ARMED_HOME  1
#define ALARM_STATE_ARMED_AWAY  2
#define ALARM_STATE_ARMED_NIGHT 3
#define ALARM_STATE_PENDING     4
#define ALARM_STATE_TRIGGERED   5

const char* const alarmStates[] = {
  "Disarmed",
  "Armed Home",
  "Armed Away",
  "Armed Night",
  "Pending",
  "Triggered"
};

const char* const alarmStateCommands[] = {
  "disarmed",
  "armed_home",
  "armed_away",
  "armed_night",
  "pending",
  "triggered"
};

const char* const alarmCommands[] = {
  "DISARM",
  "ARM_HOME",
  "ARM_AWAY",
  "ARM_NIGHT",
  "PENDING",
  "TRIGGERED",
};

const char* const sensorTypes[] = {
  "door",
  "window",
  "motion",
  "vibration",
  "sound"
};

typedef struct {
  int zonePin;
  int zoneType;
  bool zoneBypass;
  bool zoneEnable;
  bool zoneDelay;
  char zoneName[32];
  char zoneSensorType[10];
} Zone;

typedef struct {
  int alarmState;
  int lastStateChange;
  char admin_user[32];
  char admin_password[32];
  char admin_credential[64];  
} SystemInfo;

const int zoneMap[] = {
  PIN_ZONE1,
  PIN_ZONE2,
  PIN_ZONE3,
  PIN_ZONE4,
  PIN_ZONE5,
  PIN_ZONE6,
  PIN_ZONE7,
  PIN_ZONE8,
  PIN_ZONE9,
  PIN_ZONE10,
  PIN_ZONE11,
  PIN_ZONE12,
  PIN_ZONE13,
  PIN_ZONE14,
  PIN_ZONE15,
  PIN_ZONE16,
  PIN_ZONE17,
  PIN_ZONE18,
  PIN_ZONE19,
  PIN_ZONE20,
  PIN_ZONE21,
  PIN_ZONE22,
  PIN_ZONE23,
  PIN_ZONE24,
  PIN_ZONE25,
  PIN_ZONE26,
  PIN_ZONE27,
  PIN_ZONE28
}; 

#define N_ZONES sizeof zoneMap / sizeof zoneMap[0]
#define N_STATES sizeof alarmStates / sizeof alarmStates[0]
#define N_TYPES sizeof sensorTypes / sizeof sensorTypes[0]

String lastZoneState[N_ZONES];
String zoneState[N_ZONES];

SystemInfo sysInfo;
Zone zones[N_ZONES];

bool rebootFlag = false;
bool updateZoneStates = false;
bool triggeredWithDelay = false;
bool triggered = false;
                 
const int TZ_OFFSET = 5*3600;  //EST UTC-5

String MQTTServerIP;
String lastTimeStamp;
unsigned long bootTime;

long lastReconnectAttempt = 0;

void sensorTickerFunc();

Ticker sensorTicker(sensorTickerFunc, 500, 0, MILLIS);

const int NTP_PACKET_SIZE = 48;  //NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE];  //Buffer to hold incoming and outgoing packets

void(* resetFunc) (void) = 0; //declare reset function at address 0
void callback(char* p_topic, byte* p_payload, unsigned int p_length); 

EthernetClient ethClient;
PubSubClient client(ethClient);

#define PREFIX ""
WebServer webserver(PREFIX, 80);

void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  //convert topic to string to make it easier to work with
  String payload;
  String strTopic;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }
  Serial.println(F("Callback received"));
  strTopic = String((char*)p_topic);
  Serial.println(strTopic);   
  if (strTopic == MQTT_HEARTBEAT_TOPIC) {
    resetWatchdog();
    updateTelemetry(payload);
    return;
  }
  if(strTopic == MQTT_ALARM_STATE_TOPIC) {
    if(payload == alarmStateCommands[ALARM_STATE_DISARMED]) {
      Serial.println("Disarmed");
      sysInfo.alarmState = ALARM_STATE_DISARMED;
      resetAlarm();
    }
    if(payload == alarmStateCommands[ALARM_STATE_ARMED_HOME]) {
      Serial.println("Armed Home");
      sysInfo.alarmState = ALARM_STATE_ARMED_HOME;
    }
    if(payload == alarmStateCommands[ALARM_STATE_ARMED_AWAY]) {
      Serial.println("Armed Away");
      sysInfo.alarmState = ALARM_STATE_ARMED_AWAY;
    }

    if(payload == alarmStateCommands[ALARM_STATE_PENDING]) {
      Serial.println("Armed State Pending");
      sysInfo.alarmState = ALARM_STATE_PENDING;
    }

    if(payload == alarmStateCommands[ALARM_STATE_TRIGGERED]) {
      Serial.println("Triggered");
      sysInfo.alarmState = ALARM_STATE_TRIGGERED;
      triggerAlarm();
    }

  }
  Serial.print(F("MQTT Callback "));
  Serial.print(strTopic);
  Serial.print(F(" : "));
  Serial.println(payload);
}

void setup() {

  pinMode(PIN_ALARM, OUTPUT);
  pinMode(WATCHDOG, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SS, OUTPUT);
  pinMode(RST, OUTPUT);
  pinMode(CS, OUTPUT);

  pinMode(PIN_ZONE1, INPUT_PULLUP);
  pinMode(PIN_ZONE2, INPUT_PULLUP);
  pinMode(PIN_ZONE3, INPUT_PULLUP);
  pinMode(PIN_ZONE4, INPUT_PULLUP);
  pinMode(PIN_ZONE5, INPUT_PULLUP);
  pinMode(PIN_ZONE6, INPUT_PULLUP);
  pinMode(PIN_ZONE7, INPUT_PULLUP);
  pinMode(PIN_ZONE8, INPUT_PULLUP);
  pinMode(PIN_ZONE9, INPUT_PULLUP);
  pinMode(PIN_ZONE10, INPUT_PULLUP);
  pinMode(PIN_ZONE11, INPUT_PULLUP);
  pinMode(PIN_ZONE12, INPUT_PULLUP);
  pinMode(PIN_ZONE13, INPUT_PULLUP);
  pinMode(PIN_ZONE14, INPUT_PULLUP);
  pinMode(PIN_ZONE15, INPUT_PULLUP);
  pinMode(PIN_ZONE16, INPUT_PULLUP);
  pinMode(PIN_ZONE17, INPUT_PULLUP);
  pinMode(PIN_ZONE18, INPUT_PULLUP);
  pinMode(PIN_ZONE19, INPUT_PULLUP);
  pinMode(PIN_ZONE20, INPUT_PULLUP);
  pinMode(PIN_ZONE21, INPUT_PULLUP);
  pinMode(PIN_ZONE22, INPUT_PULLUP);
  pinMode(PIN_ZONE23, INPUT_PULLUP);
  pinMode(PIN_ZONE24, INPUT_PULLUP);
  pinMode(PIN_ZONE25, INPUT_PULLUP);
  pinMode(PIN_ZONE26, INPUT_PULLUP);
  pinMode(PIN_ZONE27, INPUT_PULLUP);
  pinMode(PIN_ZONE28, INPUT_PULLUP);

  digitalWrite(PIN_ZONE1, HIGH);
  digitalWrite(PIN_ZONE2, HIGH);
  digitalWrite(PIN_ZONE3, HIGH);
  digitalWrite(PIN_ZONE4, HIGH);
  digitalWrite(PIN_ZONE5, HIGH);
  digitalWrite(PIN_ZONE6, HIGH);
  digitalWrite(PIN_ZONE7, HIGH);
  digitalWrite(PIN_ZONE8, HIGH);
  digitalWrite(PIN_ZONE9, HIGH);
  digitalWrite(PIN_ZONE10, HIGH);
  digitalWrite(PIN_ZONE11, HIGH);
  digitalWrite(PIN_ZONE12, HIGH);
  digitalWrite(PIN_ZONE13, HIGH);
  digitalWrite(PIN_ZONE14, HIGH);
  digitalWrite(PIN_ZONE15, HIGH);
  digitalWrite(PIN_ZONE16, HIGH);
  digitalWrite(PIN_ZONE17, HIGH);
  digitalWrite(PIN_ZONE18, HIGH);
  digitalWrite(PIN_ZONE19, HIGH);
  digitalWrite(PIN_ZONE20, HIGH);
  digitalWrite(PIN_ZONE21, HIGH);
  digitalWrite(PIN_ZONE22, HIGH);
  digitalWrite(PIN_ZONE23, HIGH);
  digitalWrite(PIN_ZONE24, HIGH);
  digitalWrite(PIN_ZONE25, HIGH);
  digitalWrite(PIN_ZONE26, HIGH);
  digitalWrite(PIN_ZONE27, HIGH);
  digitalWrite(PIN_ZONE28, HIGH);

  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(SS, LOW);
  digitalWrite(CS, HIGH);  
  digitalWrite(RST,HIGH);
  digitalWrite(PIN_ALARM, HIGH);  
 
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect.
  }      

  Serial1.begin(115200); 
  Serial.print(F("\nMega Security initializing...  Firmware Version: "));
  Serial.println(FIRMWARE_VERSION);


  Ethernet.setHostname(MQTT_DEVICE);
  Ethernet.begin(mac);
 
  Serial.println(Ethernet.localIP());

  DNSClient dns;
  dns.begin(Ethernet.dnsServerIP());

  IPAddress result;

  char myIpString[24];

  if(dns.getHostByName(MQTT_SERVER, result) == 1) {
    Serial.print("MQTT Server IP address: ");
    Serial.println(result);
    sprintf(myIpString, "%d.%d.%d.%d", result[0], result[1], result[2], result[3]);
    MQTTServerIP = myIpString;
  }
  else Serial.print(F("dns lookup failed"));

  client.setServer(MQTTServerIP.c_str(), MQTT_PORT); //1883 is the port number you have forwared for mqtt messages. You will need to change this if you've used a different port 
  client.setCallback(callback); //callback is the function that gets called for a topic sub
  client.setBufferSize(512);

  lastReconnectAttempt = 0;

  unsigned long dataVersion;
  int address = 0;

  eeprom_read_block((void*)&dataVersion, (void*)address, sizeof(dataVersion));
  
  if(dataVersion == EEPROM_DATA_VERSION) {

    Serial.print(F("Eeprom Config Version: "));
    Serial.println(dataVersion);
    eeprom_read_block((void*)&sysInfo, (void*)(address+=sizeof(dataVersion)), sizeof(sysInfo));
    eeprom_read_block((void*)&zones, (void*)(address+=sizeof(sysInfo)), (sizeof(Zone) * N_ZONES));
  }
  else {
    dataVersion = EEPROM_DATA_VERSION;

    strcpy(sysInfo.admin_user, WEB_ADMIN_ID);
    strcpy(sysInfo.admin_password, WEB_ADMIN_PASSWORD);
    strcpy(sysInfo.admin_credential, base64::encode(String(sysInfo.admin_user) + ":" + String(sysInfo.admin_password)).c_str());
    sysInfo.alarmState = ALARM_STATE_DISARMED;
            
    // Init eeprom to default

    initZoneMap();
    eeprom_write_block((void*)&dataVersion, (void*)address, sizeof(dataVersion));
    eeprom_write_block((void*)&sysInfo, (void*)(address+=sizeof(dataVersion)), sizeof(sysInfo));
    eeprom_write_block((void*)&zones, (void *)(address+=sizeof(sysInfo)), (sizeof(Zone) * N_ZONES));
    Serial.print(F("Eeprom Config Init - Version: "));    
    Serial.println(dataVersion);    
  }

  webserver.setDefaultCommand(&defaultCmd);
  webserver.addCommand("index.html", &defaultCmd);
  webserver.addCommand("password.html", &passwordCmd);
  webserver.addCommand("zones.html", &zonesCmd);
  webserver.addCommand("system.html", &systemCmd);
  webserver.addCommand("reboot.html", &rebootCmd);

  webserver.begin();
  Serial.println(F("Web server started"));
  
  resetWatchdog();
  sensorTicker.start();
  
}

void loop() {
  char buff[64];
  int len = 64;

  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
     
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }
  else {
    // Client connected
    client.loop();
    // Update zone states on next pass
    if(updateZoneStates) {
      updateZoneStates = false;
      queryZoneStates();
    }
  }
  
  sensorTicker.update();
  webserver.processConnection(buff, &len);
}

void my_delay(unsigned long ms) {
  uint32_t start = micros();

  while (ms > 0) {
    yield();
    while ( ms > 0 && (micros() - start) >= 1000) {
      ms--;
      start += 1000;
    }
  }
}

void resetWatchdog() {
  digitalWrite(WATCHDOG, HIGH);
  my_delay(20);
  digitalWrite(WATCHDOG, LOW);
}

void triggerAlarm() {
  Serial.println(F("Alarm triggered"));  
  digitalWrite(PIN_ALARM, LOW);
  triggered = true;
}

void resetAlarm() {
  Serial.println(F("Alarm reset"));
  digitalWrite(PIN_ALARM, HIGH);
  triggered = false;
  triggeredWithDelay = false;
}

void sensorTickerFunc() {   
  checkZoneStatus();
  if((sysInfo.alarmState == ALARM_STATE_ARMED_HOME) || (sysInfo.alarmState == ALARM_STATE_ARMED_AWAY) || (sysInfo.alarmState == ALARM_STATE_ARMED_NIGHT)) {
    for( unsigned int a=0; a<N_ZONES; a++ ) {
      if(zones[a].zoneEnable) {
        if(! zones[a].zoneBypass) {
          if(zoneState[a] == "Open") {
            if(zones[a].zoneDelay && (sysInfo.alarmState == ALARM_STATE_ARMED_HOME)) {
              if (! triggeredWithDelay) {
                triggerAlarmWithDelay();         
              }
            }
            else {
              triggerAlarm();
            }
          }
        }
      }
    }
  }
}

P(Http400) = "HTTP 400 - BAD REQUEST";

P(Page_start) = "<html><head><title>Mega Security Setup</title></head>"

               "<style>"
               "ul {"
               "  list-style-type: none;"
               "  margin: 0;"
               "  padding: 0;"
               "  overflow: hidden;"
               "  background-color: #333333;"
               "}"
               "li {"
               "  float: left;"
               "}"
               "li a {"
               "  display: block;"
               "  color: white;"
               "  text-align: center;"
               "  padding: 16px;"
               "  text-decoration: none;"
               "}"
               "li a:hover {"
               "  background-color: #111111;"
               "}"
               "</style></head><body>"
               "<div id=\"main\">"
               "<ul>"
               "<li><a href=\"index.html\">Status</a></li>"
               "<li><a href=\"system.html\">Setup</a></li>"
               "<li><a href=\"zones.html\">Zones</a></li>"
               "<li><a href=\"password.html\">Admin</a></li>"
               "<li><a href=\"reboot.html\">Reboot System</a></li>"
               "</ul></div>";
               
P(Page_end) = "</body></html>";

P(Page_status_start) = "<h1>Mega Security - Status</h1>";
P(Page_system_start) = "<h1>Mega Security - Setup</h1>";
P(Page_password_start) = "<h1>Mega Security - Admin</h1>";
P(Page_zone_start) = "<h1>Mega Security - Zones</h1>";
P(Page_reboot_start) = "<h1>Mega Security - Reboot System</h1>";

P(Form_zone_start) = "<FORM action=\"zones.html\" method=\"post\">";
P(Form_zones_send) = "<INPUT type=\"submit\" value=\"Update zones\">";

P(Form_end) = "</FORM>\n";
P(Form_input_text_start) = "<input type=\"text\" size=\"30\" name=\"";
P(Form_input_password_start) = "<input type=\"password\" name=\"";
P(Form_input_value) = "\" value=\"";
P(Form_input_end) = "\">\n";
P(Form_select_start) = "<select name=\"";
P(Form_option_start) = "<option value=\"";
P(Form_option_mid) = "\">";
P(Form_option_mid_selected) = "\" selected>";
P(Form_option_end) = "</option>\n";
P(Form_select_end) = "</select>\n";
P(Form_reboot_start) = "<FORM action=\"reboot.html\" method=\"post\">\n";
P(Form_password_start) = "<FORM action=\"password.html\" method=\"post\">\n";
P(Form_password_send) = "<INPUT type=\"submit\" value=\"Reset Password\">\n";
P(Form_reboot_send) = "<INPUT type=\"submit\" value=\"Reboot System\">\n";

P(Page_password_reset) = "<h1>Mega Security - Admin Password Reset!!</h1>";
P(Page_restart) = "<h1>Mega Security - System is Rebooting....</h1>";
P(Page_old_password_nomatch) = "<h1>Mega Security - Old password does not match!!</h1>";
P(Page_new_password_nomatch) = "<h1>Mega Security - New password does not match!!</h1>";
P(firmware_version) = "Firmware Version: ";

P(zone_heading) = "<b>Zone</b>";
P(zone_name) = "<b>Name</b>";
P(zone_delay) = "<b>Delay</b>";
P(zone_bypass) = "<b>Bypass</b>";
P(zone_pin) = "<b>Pin</b>";
P(zone_pin_state) = "<b>State</b>";
P(zone_type) = "<b>Type</b>";

P(no_zones_enabled) = "<b>No Zones Enabled!</b>";

P(yes) = "Yes";
P(no) = "No";

P(br) = "<br>\n";
P(h2_start) = "<h2>";
P(h2_end) = "</h2>";
P(h3_start) = "<h3>";
P(h3_end) = "</h3>";
P(table_start) = "<table>";
P(table_col_width_start) = "<col width=\"";
P(table_tr_start) = "<tr>";
P(table_tr_end) = "</tr>\n";
P(table_td_start) = "<td>";
P(table_td_end) = "</td>\n";
P(table_end) = "</table>\n";

P(Form_cb_start) = "<input type=\"checkbox\" name=\"";
P(Form_cb_value) = "\" value=\"";
P(close_bracket) = "\">\n";
P(Form_cb_checked) = "\" checked>\n";
P(Form_cb_enable) = "<label>Enable Zone</label>\n";
P(Form_cb_bypass) = "<label>Bypass Zone</label>\n";
P(Form_cb_delay) = "<label>Delay Zone</label>\n";

void defaultCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {

  bool availableZones=false;

  if (server.checkCredentials(sysInfo.admin_credential)) {
    server.httpSuccess();

    if (type == WebServer::HEAD) {
      return;
    }
    
    server.printP(Page_start);
    server.printP(Page_status_start);
    server.printP(firmware_version);
    server.print(FIRMWARE_VERSION);
    server.printP(br);
    for( unsigned int a=0; a<N_ZONES; a++ ) {
      if (zones[a].zoneEnable) {
        availableZones = true;
      }
    }
    
    if(availableZones) {
      server.printP(table_start);
      server.printP(table_col_width_start);
      server.print("80");
      server.printP(close_bracket);
      server.printP(table_col_width_start);
      server.print("240");
      server.printP(close_bracket);
      server.printP(table_col_width_start);
      server.print("80");
      server.printP(close_bracket);
      server.printP(table_col_width_start);
      server.print("80");
      server.printP(close_bracket);
      server.printP(table_col_width_start);
      server.print("80");
      server.printP(close_bracket);
      server.printP(table_col_width_start);
      server.print("80");
      server.printP(close_bracket);
      server.printP(table_col_width_start);
      server.print("80");
      server.printP(close_bracket);

      server.printP(table_tr_start);
      server.printP(table_td_start);
      server.printP(zone_heading);
      server.printP(table_td_end);
      server.printP(table_td_start);
      server.printP(zone_name);
      server.printP(table_td_end);
      server.printP(table_td_start);
      server.printP(zone_delay);
      server.printP(table_td_end);
      server.printP(table_td_start);
      server.printP(zone_bypass);
      server.printP(table_td_end);
      server.printP(table_td_start);
      server.printP(zone_pin);
      server.printP(table_td_end);
      server.printP(table_td_start);
      server.printP(zone_pin_state);
      server.printP(table_td_end);
      server.printP(table_td_start);
      server.printP(zone_type);
      server.printP(table_td_end);
      server.printP(table_tr_end);
    
      for( unsigned int a=0; a<N_ZONES; a++ ) {
        if(zones[a].zoneEnable) {
          server.printP(table_tr_start);
          server.printP(table_td_start);
          server.print(String("Zone ") + String(a+1));
          server.printP(table_td_end);
          server.printP(table_td_start);
          server.print(zones[a].zoneName);
          server.printP(table_td_end);      
          server.printP(table_td_start);
          if(zones[a].zoneDelay) {
            server.printP(yes);
          }
          else {
            server.printP(no);
          }
          server.printP(table_td_end);
          server.printP(table_td_start);
          if(zones[a].zoneBypass) {
            server.printP(yes);
          }
          else {
            server.printP(no);
          }
          server.printP(table_td_end);
          server.printP(table_td_start);
          server.print(zones[a].zonePin);
          server.printP(table_td_end);
          server.printP(table_td_start);
          server.print(getCurrentState(zones[a].zonePin).c_str());
          server.printP(table_td_end);
          server.printP(table_td_start);
          server.print(zones[a].zoneSensorType);
          server.printP(table_td_end);
        }
      }
    
      server.printP(table_end);    
    }
    else {
      server.printP(br);
      server.printP(no_zones_enabled);
    }
    server.printP(Page_end);
  }
  else {
    server.httpUnauthorized();
  }
}

void systemCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {

  if (server.checkCredentials(sysInfo.admin_credential)) {
    server.httpSuccess();

    if (type == WebServer::HEAD) {
      return;
    }
    
    server.printP(Page_start);
    server.printP(Page_system_start);
    server.printP(Page_end);
  }
  else {
    server.httpUnauthorized();
  }
}

void passwordCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {

  if (server.checkCredentials(sysInfo.admin_credential)) {
    server.httpSuccess();

    if (type == WebServer::HEAD) {
      return;
    }

    if (type == WebServer::POST) {
      char name[16], value[32];
      bool repeat;
      char newPassword[32];
      char oldPassword[32];
      char confirmPassword[32];
       
      do {
        repeat = server.readPOSTparam(name, 16, value, 32);

        if (strstr(name, "old")) {
          strcpy(oldPassword, value);
        }
        if (strstr(name, "new")) {
          strcpy(newPassword, value);
        }
        if (strstr(name, "confirm")) {
          strcpy(confirmPassword, value+1);
          
        }
        
      } while (repeat);

      if( strcmp(oldPassword, sysInfo.admin_password)) {
        server.printP(Page_old_password_nomatch);
        return;
      }
      else if( strcmp(newPassword, confirmPassword)) {
        server.printP(Page_new_password_nomatch);
        return;
      }
      else {
        strcpy(sysInfo.admin_password, newPassword);
        
        strcpy(sysInfo.admin_credential, base64::encode(String(sysInfo.admin_user) + ":" + String(sysInfo.admin_password)).c_str());
        server.printP(Page_password_reset);
        return;           
      }
      writeToEeprom();

    } 

    server.printP(Page_start);
    server.printP(Page_password_start);
    server.printP(Form_password_start);
    server.printP(table_start);
    server.printP(table_tr_start);
    server.printP(table_td_start);
    server.print("Old Password ");
    server.printP(table_td_end);
    server.printP(table_td_start);
    server.printP(Form_input_password_start);
    server.print("old_password");
    server.printP(Form_input_value);
    server.printP(Form_input_end);     
    server.printP(table_td_end);
    server.printP(table_tr_end);   
    server.printP(table_tr_start);
    server.printP(table_td_start);
    server.print("New Password ");
    server.printP(table_td_end);
    server.printP(table_td_start);
    server.printP(Form_input_password_start);
    server.print("new_password");
    server.printP(Form_input_value);
    server.printP(Form_input_end);     
    server.printP(table_td_end);
    server.printP(table_tr_end);
    server.printP(table_tr_start);
    server.printP(table_td_start);
    server.print("Confirm Password ");
    server.printP(table_td_end);
    server.printP(table_td_start);
    server.printP(Form_input_password_start);
    server.print("confirm_password");
    server.printP(Form_input_value);
    server.printP(Form_input_end);     
    server.printP(table_td_end);
    server.printP(table_tr_end);          
    server.printP(table_end);    
    server.printP(Form_password_send);
    server.printP(Form_end);
    server.printP(Page_end);
  }
  else {
    server.httpUnauthorized();
  }
}

void zonesCmd(WebServer &server, WebServer::ConnectionType type, char *, bool) {
   
  if (server.checkCredentials(sysInfo.admin_credential)) {
    server.httpSuccess();

    if (type == WebServer::HEAD) {
      return;
    }
    
    if (type == WebServer::POST) {
      char name[16], value[32];
      bool repeat;

      initZoneMap();
       
      do {
        repeat = server.readPOSTparam(name, 16, value, 32);

        if (strstr(name, "zone_")) {
          int zone = atoi(name+5);
          strcpy(zones[zone].zoneName, value);
        }
        if (strstr(name, "eid")) {
          int zone = atoi(name+4);
          zones[zone].zoneEnable = true;
        }
        if (strstr(name, "bid")) {
          int zone = atoi(name+4);
          zones[zone].zoneEnable = true;
          zones[zone].zoneBypass = true;
        }
        if (strstr(name, "did")) {
          int zone = atoi(name+4);
          zones[zone].zoneEnable = true;
          zones[zone].zoneDelay = true;
        }
        if (strstr(name, "zonetype")) {
          int zone = atoi(name+9);
          strcpy(zones[zone].zoneSensorType, value);
        }        
        
      } while (repeat);
      writeToEeprom();
      updateHomeAssistant();
      updateZoneStates = true;
    }  

    server.printP(Page_start);
    server.printP(Page_zone_start);
    server.printP(Form_zone_start);
    server.printP(table_start);
    server.printP(table_col_width_start);
    server.print("80");
    server.printP(close_bracket);
    server.printP(table_col_width_start);
    server.print("240");
    server.printP(close_bracket);
    server.printP(table_col_width_start);
    server.print("120");
    server.printP(close_bracket);
    server.printP(table_col_width_start);
    server.print("120");
    server.printP(close_bracket);
    server.printP(table_col_width_start);
    server.print("120");
    server.printP(close_bracket);
    server.printP(table_col_width_start);
    server.print("100");
    server.printP(close_bracket);

    for( unsigned int a=0; a<N_ZONES; a++ ) {
      server.printP(table_tr_start);
      server.printP(table_td_start);
      server.print(String("Zone ") + String(a+1));
      server.printP(table_td_end);
      server.printP(table_td_start);
      server.printP(Form_input_text_start);
      server.print(String("zone_") + String(a));
      server.printP(Form_input_value);
      server.print(zones[a].zoneName);
      server.printP(Form_input_end);     
      server.printP(table_td_end);
      
      server.printP(table_td_start);
      server.printP(Form_cb_start);
      server.print("eid_" + String(a));
      server.printP(Form_cb_value);
      server.print("enable");
      if(zones[a].zoneEnable) {
        server.printP(Form_cb_checked);
      }
      else {
        server.printP(close_bracket); 
      }
      server.printP(Form_cb_enable); 
      server.printP(table_td_end);

      server.printP(table_td_start);
      server.printP(Form_cb_start);
      server.print("bid_" + String(a));
      server.printP(Form_cb_value);
      server.print("enable");
      if(zones[a].zoneBypass) {
        server.printP(Form_cb_checked);
      }
      else {
        server.printP(close_bracket);
      }
      server.printP(Form_cb_bypass); 
      server.printP(table_td_end);

      server.printP(table_td_start);
      server.printP(Form_cb_start);
      server.print("did_" + String(a));
      server.printP(Form_cb_value);
      server.print("enable");
      if(zones[a].zoneDelay) {
        server.printP(Form_cb_checked);
      }
      else {
        server.printP(close_bracket);   
      }
      server.printP(Form_cb_delay); 
      server.printP(table_td_end);

      server.printP(table_td_start);
      server.printP(Form_select_start);
      server.print(String("zonetype_") + String(a));
      server.printP(Form_option_mid);
      for( unsigned int b=0; b<N_TYPES; b++ ) {
        server.printP(Form_option_start);
        server.print(sensorTypes[b]);
        if(strcmp(sensorTypes[b], zones[a].zoneSensorType)) {
          server.printP(Form_option_mid);
        }
        else {
          server.printP(Form_option_mid_selected);            
        }
        server.print(sensorTypes[b]);        
        server.printP(Form_option_end);
      }
      server.printP(Form_select_end);
      server.printP(table_td_end);
      
      server.printP(table_tr_end);
    }

    server.printP(table_end);
    server.printP(Form_zones_send);
    server.printP(Form_end);
    server.printP(br);
    server.printP(Page_end);

  }
  else {
    server.httpUnauthorized();
  }
}

void rebootCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {

  if (server.checkCredentials(sysInfo.admin_credential)) {
    server.httpSuccess();

    if (type == WebServer::HEAD) {
      return;
    }

    if (type == WebServer::POST) {
      server.printP(Page_restart);
      rebootFlag = true;
      return;
    }  

    server.printP(Page_start);
    server.printP(Page_reboot_start);
    server.printP(Form_reboot_start);
    server.printP(table_start);
    server.printP(Form_reboot_send);
    server.printP(table_end);
    server.printP(Form_end);
    server.printP(br);
    server.printP(Page_end);
    
  }
  else {
    server.httpUnauthorized();
  }
}

void writeToEeprom() {
  unsigned long dataVersion = EEPROM_DATA_VERSION;
  int address = 0;
  
  eeprom_write_block((void*)&dataVersion, (void*)address, sizeof(dataVersion));
  eeprom_write_block((void*)&sysInfo, (void*)(address+=sizeof(dataVersion)), sizeof(sysInfo));
  eeprom_write_block((void*)&zones, (void *)(address+=sizeof(sysInfo)), (sizeof(Zone) * N_ZONES));
}

String getCurrentState(int pin) {
  String state;
  int val;
  val = digitalRead(pin);
  if(val == LOW) {
    state = "Closed";
  }
  else {
    state = "Open";    
  }
  return state;
}

void checkZoneStatus() {
  
  for( unsigned int a=0; a<N_ZONES; a++ ) {
    if(zones[a].zoneEnable) {
      
      zoneState[a] = getCurrentState(zoneMap[a]);
      if (zoneState[a] != lastZoneState[a]) {
        lastZoneState[a] = zoneState[a];
        String topic = String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) + "/state";
        Serial.print(F("MQTT - "));
        Serial.print(topic);
        Serial.print(" : ");
        if(zoneState[a] == "Closed") {
          client.publish(topic.c_str(), "OFF");
          Serial.println(F("off"));
        }
        else {
          client.publish(topic.c_str(), "ON");            
          Serial.println(F("on"));
        }
      }
    }
  }  
}

void queryZoneStates() {
  for( unsigned int a=0; a<N_ZONES; a++ ) {
    if(zones[a].zoneEnable) {

      String zoneBypass = "OFF";
      String zoneDelay = "OFF";

      if(zones[a].zoneBypass == true) {
        zoneBypass = "ON";
      }
      if(zones[a].zoneDelay == true) {
        zoneDelay = "ON";
      }

      String topic = String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) + "_delay/state";
      String message = zoneDelay;
      
      Serial.print(F("MQTT - "));
      Serial.print(topic);
      Serial.print(F(" : "));
      Serial.println(message.c_str());
      client.publish(topic.c_str(), message.c_str(), true);

      topic = String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) + "_bypass/state";
      message = zoneBypass;
      Serial.print(F("MQTT - "));
      Serial.print(topic);
      Serial.print(F(" : "));
      Serial.println(message.c_str());
      client.publish(topic.c_str(), message.c_str(), true);

    }
  }
}

void registerTelemetry() {
  String topic = String(MQTT_DISCOVERY_SENSOR_PREFIX) + HA_TELEMETRY + "-" + String(MQTT_DEVICE) + "/config";
  String message = String("{\"name\": \"") + HA_TELEMETRY + "-" + MQTT_DEVICE +
                   String("\", \"json_attributes_topic\": \"") + String(MQTT_DISCOVERY_SENSOR_PREFIX) + HA_TELEMETRY + "-" + String(MQTT_DEVICE) + 
                   String("/attributes\", \"state_topic\": \"") + String(MQTT_DISCOVERY_SENSOR_PREFIX) + HA_TELEMETRY + "-" + String(MQTT_DEVICE) +
                   String("/state\"}");
  Serial.print(F("MQTT - "));
  Serial.print(topic);
  Serial.print(F(" : "));
  Serial.println(message.c_str());

  client.publish(topic.c_str(), message.c_str(), true);  
  
}

void updateTelemetry(String heartbeat) {

  uptime::calculateUptime();

  byte macBuffer[6];  // create a buffer to hold the MAC address
  Ethernet.macAddress(macBuffer); // fill the buffer 
  String mac_address;
  for (byte octet = 0; octet < 6; octet++) {    
    mac_address += String(macBuffer[octet], HEX);
    if (octet < 5) {
      mac_address += "-";
    }
  }
  
  String topic = String(MQTT_DISCOVERY_SENSOR_PREFIX) + HA_TELEMETRY + "-" + String(MQTT_DEVICE) + "/attributes";
  String message = String("{\"firmware\": \"") + FIRMWARE_VERSION  +
            String("\", \"mac_address\": \"") + mac_address +
            String("\", \"mqtt_server\": \"") + MQTTServerIP +
            String("\", \"compile_date\": \"") + compile_date +
            String("\", \"heartbeat\": \"") + heartbeat +
            String("\", \"uptime\": \"") + uptime::getDays() + "d:" + uptime::getHours() + "h:" + uptime::getMinutes() + "m:" + uptime::getSeconds() + "s" +
            String("\", \"ip_address\": \"") + ip2Str(Ethernet.localIP()) + String("\"}");
  Serial.print(F("MQTT - "));
  Serial.print(topic);
  Serial.print(F(" : "));
  Serial.println(message);
  client.publish(topic.c_str(), message.c_str(), true);

  topic = String(MQTT_DISCOVERY_SENSOR_PREFIX) + HA_TELEMETRY + "-" + String(MQTT_DEVICE) + "/state";
  message = String(MQTT_DEVICE);
  Serial.print(F("MQTT - "));
  Serial.print(topic);
  Serial.print(F(" : "));
  Serial.println(message);
  client.publish(topic.c_str(), message.c_str(), true);

}

String ip2Str(IPAddress ip){
  String s="";
  for (int i=0; i<4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
}

void updateHomeAssistant() {
  for( unsigned int a=0; a<N_ZONES; a++ ) {
    if(zones[a].zoneEnable) {

      String zoneBypass = "off";
      String zoneDelay = "off";
      
      String topic = String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) + "/config";
      String message = String("{\"name\": \"") + zones[a].zoneName +
                       String("\", \"json_attributes_topic\": \"") + String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) + 
                       String("/attributes\", \"state_topic\": \"") + String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) +
                       String("/state\", \"device_class\": \"") + zones[a].zoneSensorType + String("\"}");
      Serial.print(F("MQTT - "));
      Serial.print(topic);
      Serial.print(F(" : "));
      Serial.println(message.c_str());
      client.publish(topic.c_str(), message.c_str(), true);

      if(zones[a].zoneBypass == true) {
        zoneBypass = "on";
      }
      if(zones[a].zoneDelay == true) {
        zoneDelay = "on";
      }

      topic = String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) + "_delay/config";
      message = String("{\"name\": \"") + zones[a].zoneName +
                       String(" Delay\", \"json_attributes_topic\": \"") + String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) + 
                       String("_delay/attributes\", \"state_topic\": \"") + String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) +
                       String("_delay/state\"}");
      Serial.print(F("MQTT - "));
      Serial.print(topic);
      Serial.print(F(" : "));
      Serial.println(message.c_str());
      client.publish(topic.c_str(), message.c_str(), true);

      topic = String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) + "_bypass/config";
      message = String("{\"name\": \"") + zones[a].zoneName +
                       String(" Bypass\", \"json_attributes_topic\": \"") + String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) + 
                       String("_bypass/attributes\", \"state_topic\": \"") + String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) +
                       String("_bypass/state\"}");
      Serial.print(F("MQTT - "));
      Serial.print(topic);
      Serial.print(F(" : "));
      Serial.println(message.c_str());
      client.publish(topic.c_str(), message.c_str(), true);
      
      topic = String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) + "/attributes";
      message = String("{\"delay\": \"") + zoneDelay + String("\", \"bypass\": \"") + zoneBypass + String("\", \"zone_id\": \"") + String(a) + String("\"}");

      Serial.print(F("MQTT - "));
      Serial.print(topic);
      Serial.print(F(" : "));
      Serial.println(message);
      client.publish(topic.c_str(), message.c_str(), true);

      topic = String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) + "_bypass/attributes";

      Serial.print(F("MQTT - "));
      Serial.print(topic);
      Serial.print(F(" : "));
      Serial.println(message);
      client.publish(topic.c_str(), message.c_str(), true);

      topic = String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + "zone_" + String(a) + "_delay/attributes";

      Serial.print(F("MQTT - "));
      Serial.print(topic);
      Serial.print(F(" : "));
      Serial.println(message);
      client.publish(topic.c_str(), message.c_str(), true);
      
    }
  } 
}

void initZoneMap() {
  for( unsigned int a=0; a<N_ZONES; a++ ) {
    char temp[4];
    
    zones[a].zonePin = zoneMap[a];
    zones[a].zoneEnable = false;
    zones[a].zoneBypass = false;
    zones[a].zoneDelay = false;
    strcpy(zones[a].zoneName, "Zone ");
    strcat(zones[a].zoneName, itoa(a+1, temp, 10));
    strcpy(zones[a].zoneSensorType, sensorTypes[0]);
  }  
}

boolean reconnect() {
  //Reconnect to MQTT
  if (client.connect(MQTT_DEVICE, MQTT_USER, MQTT_PASSWORD)) {
    Serial.print(F("Attempting MQTT connection..."));
    Serial.println(F("connected"));
    client.subscribe(MQTT_HEARTBEAT_SUB);
    client.subscribe(MQTT_ALARM_STATE_TOPIC);    
    String firmwareVer = String(F("Firmware Version: ")) + String(FIRMWARE_VERSION);
    String compileDate = String(F("Build Date: ")) + String(compile_date);
    updateHomeAssistant();
    updateZoneStates = true;
    registerTelemetry();
    updateTelemetry("Unknown");
  }
  return client.connected();
}

void  triggerAlarmWithDelay() {
  Serial.println("Alarm triggered with delay");
}
