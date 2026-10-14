#ifndef NET_MQTT_H
#define NET_MQTT_H

#include <Arduino.h>

void NetMqtt_Init(void);
void NetMqtt_Step(void);
bool NetMqtt_PublishVMC(const char *jsonPayload);

#endif /* NET_MQTT_H */