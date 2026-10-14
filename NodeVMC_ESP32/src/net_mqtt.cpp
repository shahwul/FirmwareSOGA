#include "net_mqtt.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static WiFiClient        s_espClient;
static PubSubClient      s_mqtt(s_espClient);
static SemaphoreHandle_t s_mqttMutex = NULL;
static int               s_failCount = 0;

void NetMqtt_Init(void) {
    s_mqttMutex = xSemaphoreCreateMutex();

    pinMode(LED_STATUS_PIN, OUTPUT);
    digitalWrite(LED_STATUS_PIN, HIGH);

    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.setAutoReconnect(true);

    s_mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    s_mqtt.setBufferSize(MQTT_BUFFER_SIZE);
    s_mqtt.setKeepAlive(MQTT_KEEPALIVE_SEC);
}

void NetMqtt_Step(void) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[WIFI] Putus! Menghubungkan ke jaringan...");
        WiFi.disconnect(true);
        vTaskDelay(pdMS_TO_TICKS(500));
        WiFi.begin(WIFI_SSID, WIFI_PASS);

        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 15) {
            digitalWrite(LED_STATUS_PIN, LOW);
            vTaskDelay(pdMS_TO_TICKS(150));
            digitalWrite(LED_STATUS_PIN, HIGH);
            vTaskDelay(pdMS_TO_TICKS(350));
            retry++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("[WIFI] Terhubung! IP: ");
            Serial.println(WiFi.localIP());
            s_failCount = 0;
        }
        return;
    }

    if (xSemaphoreTake(s_mqttMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!s_mqtt.connected()) {
            if (s_mqtt.connect(NODE_ID)) {
                Serial.println("[MQTT] Terhubung ke Broker!");
                s_failCount = 0;
            } else {
                s_failCount++;
                if (s_failCount >= 3) {
                    WiFi.disconnect();
                    s_failCount = 0;
                }
            }
        } else {
            s_mqtt.loop();
            digitalWrite(LED_STATUS_PIN, ((millis() / 1000) % 2 == 0) ? LOW : HIGH);
        }
        xSemaphoreGive(s_mqttMutex);
    }
}

bool NetMqtt_PublishVMC(const char *jsonPayload) {
    if (WiFi.status() != WL_CONNECTED || !s_mqtt.connected()) {
        return false;
    }

    bool ok = false;
    if (xSemaphoreTake(s_mqttMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ok = s_mqtt.publish(TOPIC_PUB_VMC_TELEMETRY, jsonPayload);
        xSemaphoreGive(s_mqttMutex);
    }
    return ok;
}