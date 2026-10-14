#include "net_manager.h"
#include "config.h"
#include "stm32_bridge.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static WiFiClient        s_espClient;
static PubSubClient      s_mqtt(s_espClient);
static SemaphoreHandle_t s_mqttMutex = NULL;
static int               s_mqttFailCount = 0;

/* Callback saat ada pesan masuk dari Broker MQTT */
static void onMqttCallback(char *topic, byte *payload, unsigned int length) {
    // Jangan teruskan pesan milik Node 2 (diri sendiri) agar tidak looping
    if (strcmp(topic, TOPIC_PUB_TELEMETRY) == 0) {
        return;
    }

    char msg[MQTT_BUFFER_SIZE];
    size_t copyLen = (length < (sizeof(msg) - 1)) ? length : (sizeof(msg) - 1);
    memcpy(msg, payload, copyLen);
    msg[copyLen] = '\0';

    Serial.printf("\n[MQTT SUB] (%s) -> %s\n", topic, msg);

    if (STM32Bridge_SendLine(msg)) {
        Serial.println("[FORWARDER] Sukses diteruskan ke STM32!");
    } else {
        Serial.println("[FORWARDER] Gagal kirim ke STM32 (UART Sibuk)!");
    }
}

void NetManager_Init(void) {
    s_mqttMutex = xSemaphoreCreateMutex();

    pinMode(LED_STATUS_PIN, OUTPUT);
    digitalWrite(LED_STATUS_PIN, HIGH); // Mati (Active LOW)

    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm); // Penting untuk desain antena ESP32-C3 SuperMini
    WiFi.setAutoReconnect(true);

    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);

    s_mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    s_mqtt.setBufferSize(MQTT_BUFFER_SIZE); // Wajib 512 agar paket heartbeat tidak terpotong
    s_mqtt.setKeepAlive(MQTT_KEEPALIVE_SEC);
    s_mqtt.setCallback(onMqttCallback);
}

bool NetManager_IsConnected(void) {
    return (WiFi.status() == WL_CONNECTED) && s_mqtt.connected();
}

void NetManager_Step(void) {
    /* 1. CEK KONEKSI WIFI */
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[NETWORK] WiFi Putus! Menghubungkan ulang...");
        WiFi.disconnect(true);
        vTaskDelay(pdMS_TO_TICKS(500));

        #if USE_BSSID_LOCK
        WiFi.begin(WIFI_SSID, WIFI_PASS, 0, ROUTER_BSSID);
        #else
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        #endif

        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 15) {
            digitalWrite(LED_STATUS_PIN, LOW);
            vTaskDelay(pdMS_TO_TICKS(200));
            digitalWrite(LED_STATUS_PIN, HIGH);
            vTaskDelay(pdMS_TO_TICKS(500));
            retry++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("\n[NETWORK] WiFi Terhubung! IP: ");
            Serial.println(WiFi.localIP());
            s_mqttFailCount = 0;
        }
        return;
    }

    /* 2. CEK KONEKSI BROKER MQTT */
    if (xSemaphoreTake(s_mqttMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!s_mqtt.connected()) {
            Serial.println("[MQTT] Menghubungkan ke Broker...");
            if (s_mqtt.connect(NODE_ID)) {
                Serial.println("[MQTT] Terhubung! Subscribe topik...");
                s_mqtt.subscribe(TOPIC_SUB_SENSOR_ALL);
                s_mqtt.subscribe(TOPIC_SUB_COMMAND);
                s_mqttFailCount = 0;
            } else {
                s_mqttFailCount++;
                Serial.printf("[MQTT] Gagal konek (rc=%d), percobaan ke-%d\n",
                              s_mqtt.state(), s_mqttFailCount);
                if (s_mqttFailCount >= 3) {
                    WiFi.disconnect();
                    s_mqttFailCount = 0;
                }
            }
        } else {
            s_mqtt.loop();
            // Kedip lambat 1 Hz tanda sistem online & sehat
            digitalWrite(LED_STATUS_PIN, ((millis() / 1000) % 2 == 0) ? LOW : HIGH);
        }
        xSemaphoreGive(s_mqttMutex);
    }
}

void NetManager_RouteUpstreamJSON(const char *jsonLine) {
    if (!NetManager_IsConnected()) {
        Serial.println("[MQTT PUB] Gagal lapor, Broker sedang putus!");
        return;
    }

    const char *targetTopic = NULL;

    // 1. Jalur ACK perintah manual
    if (strstr(jsonLine, "\"type\":\"ack\"") != NULL ||
        strstr(jsonLine, "\"siram_selesai\"") != NULL) {
        targetTopic = TOPIC_PUB_ACK;
    }
    // 2. Jalur Control State & Heartbeat Telemetry
    else if (strstr(jsonLine, "\"type\":\"control_state\"") != NULL ||
             strstr(jsonLine, "\"type\":\"heartbeat\"") != NULL ||
             strstr(jsonLine, "\"uptime\"") != NULL) {
        targetTopic = TOPIC_PUB_TELEMETRY;
    }

    if (targetTopic == NULL) {
        Serial.printf("[WARNING] JSON tidak dikenali, di-drop: %s\n", jsonLine);
        return;
    }

    if (xSemaphoreTake(s_mqttMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool ok = s_mqtt.publish(targetTopic, jsonLine);
        xSemaphoreGive(s_mqttMutex);

        if (ok) {
            Serial.printf("[MQTT PUB] Terkirim ke (%s)\n", targetTopic);
        } else {
            Serial.println("[MQTT PUB] ERROR: Gagal publish (cek ukuran buffer)!");
        }
    }
}

void NetManager_SyncTimeToSTM32(void) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[NTP SYNC] Skip: WiFi belum terhubung.");
        return;
    }

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 2000)) {
        char payloadTime[80];
        snprintf(payloadTime, sizeof(payloadTime),
                 "{\"cmd\":\"sync_rtc\",\"h\":%d,\"m\":%d,\"s\":%d}",
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        STM32Bridge_SendLine(payloadTime);

        Serial.println("\n==================================================");
        Serial.printf("[NTP SYNC] Waktu WIB -> %02d:%02d:%02d\n",
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        Serial.printf("[NTP SYNC] Disuntikkan ke STM32: %s\n", payloadTime);
        Serial.println("==================================================");
    } else {
        Serial.println("[NTP SYNC] Warning: Gagal mengambil waktu dari server NTP.");
    }
}