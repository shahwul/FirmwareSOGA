#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ================= 1. IDENTITAS NODE & WIFI ================= */
#define NODE_ID                 "Soga_Tank_Node"
#define WIFI_SSID               "tempe kanget"
#define WIFI_PASS               "12345678"

// Set ke true jika ingin mengunci koneksi hanya ke BSSID router tertentu
#define USE_BSSID_LOCK          false
static const uint8_t ROUTER_BSSID[6] = {0x5C, 0x02, 0x14, 0xED, 0xAA, 0x98};

/* ================= 2. KONFIGURASI BROKER & TOPIK MQTT ======= */
#define MQTT_SERVER             "10.161.28.83"
#define MQTT_PORT               1883
#define MQTT_BUFFER_SIZE        512
#define MQTT_KEEPALIVE_SEC      30

// Topik Inbound (ESP32-C3 Subscribe -> Teruskan ke STM32F401)
// Catatan: Menggunakan wildcard '+' agar otomatis menerima data dari Node 1 (DHT/VMC)
// maupun node sensor tambahan, ditambah topik spesifik perintah pompa/valve.
#define TOPIC_SUB_SENSOR_ALL    "gh/GH-001/node/+/telemetry"
#define TOPIC_SUB_COMMAND       "gh/GH-001/command/water/2"

// Topik Outbound (STM32F401 -> ESP32-C3 Publish ke Broker)
#define TOPIC_PUB_ACK           "gh/GH-001/command/water/2/ack"
#define TOPIC_PUB_TELEMETRY     "gh/GH-001/node/2/telemetry"

/* ================= 3. KONFIGURASI PIN HARDWARE ============== */
#define LED_STATUS_PIN          8   // LED bawaan ESP32-C3 SuperMini (Active LOW)
#define STM32_UART_RX_PIN       9   // Terhubung ke PA2 (USART2_TX STM32F401)
#define STM32_UART_TX_PIN       10  // Terhubung ke PA3 (USART2_RX STM32F401)
#define STM32_UART_BAUD         115200

/* ================= 4. KONFIGURASI NTP (WIB / UTC+7) ========= */
#define NTP_SERVER_1            "pool.ntp.org"
#define NTP_SERVER_2            "id.pool.ntp.org"
#define GMT_OFFSET_SEC          (7L * 3600L)
#define DAYLIGHT_OFFSET_SEC     0
#define NTP_SYNC_INTERVAL_MS    60000UL // Suntik RTC ke STM32 tiap 60 detik

#endif /* CONFIG_H */