#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#define MQTT_ADDRESS   "tcp://localhost:1883"
#define MQTT_CLIENT_ID "OTA_SERVER"
#define MQTT_TOPIC     "ota/update"
#define MQTT_QOS       1
#define MQTT_TIMEOUT   1000L

void publish_update_notification(const char* address, const char* version);

#endif