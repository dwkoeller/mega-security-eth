#ifndef CREDENTIALS_H
#define CREDENTIALS_H
byte mac[]     = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip   (192, 168, 1, 150);
IPAddress dns  (192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
#define MQTT_SERVER      "192.168.1.10"
#define MQTT_PORT        1883
#define MQTT_USER        "mqttuser"
#define MQTT_PASSWORD    "mqttpassword"
#define WEB_ADMIN_ID       "admin"
#define WEB_ADMIN_PASSWORD "password"
#define EXIT_DELAY_SECONDS  30
#define ENTRY_DELAY_SECONDS 20
#define TOTAL_ZONES 16
#endif