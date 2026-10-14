#ifndef NET_MANAGER_H
#define NET_MANAGER_H

#include <Arduino.h>

void NetManager_Init(void);
void NetManager_Step(void);             // Dipanggil berkala di TaskNetwork
bool NetManager_IsConnected(void);

// Merutekan JSON dari STM32 ke topik MQTT (ACK vs Telemetry/Heartbeat)
void NetManager_RouteUpstreamJSON(const char *jsonLine);

// Menyuntikkan waktu NTP terbaru ke RTC STM32
void NetManager_SyncTimeToSTM32(void);

#endif /* NET_MANAGER_H */