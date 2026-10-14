#include <Arduino.h>
#include "config.h"
#include "stm8_parser.h"
#include "net_mqtt.h"

static void TaskNetwork(void *pvParameters) {
    (void)pvParameters;
    for (;;) {
        NetMqtt_Step();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void TaskVmcBridge(void *pvParameters) {
    (void)pvParameters;
    VmcTelemetry_t vmcData = {0};
    char jsonPayload[MQTT_BUFFER_SIZE];

    for (;;) {
        if (STM8Parser_Poll(&vmcData)) {
            STM8Parser_BuildJSON(&vmcData, jsonPayload, sizeof(jsonPayload));
            Serial.printf("[VMC RX] %s\n", jsonPayload);

            if (NetMqtt_PublishVMC(jsonPayload)) {
                Serial.println("[MQTT PUB] Telemetri 5 VMC terkirim!");
            } else {
                Serial.println("[MQTT PUB] Tertunda (Broker belum terhubung)");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void setup() {
    delay(1500);
    Serial.begin(115200);

    Serial.println("\n==============================================");
    Serial.println("   SOGA FARM - NODE VMC GATEWAY (ESP32-C3)    ");
    Serial.println("==============================================");

    STM8Parser_Init();
    NetMqtt_Init();

    xTaskCreatePinnedToCore(TaskNetwork,   "Net_Task", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(TaskVmcBridge, "VMC_Task", 4096, NULL, 2, NULL, 0);
}

void loop() {
    vTaskDelete(NULL);
}