#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ================= 1. KONFIGURASI WIFI & BROKER MQTT ================ */
#define NODE_ID                 "Soga_VMC_Node"
#define WIFI_SSID               "tempe kanget"
#define WIFI_PASS               "12345678"

#define MQTT_SERVER             "10.161.28.83"
#define MQTT_PORT               1883
#define MQTT_BUFFER_SIZE        512
#define MQTT_KEEPALIVE_SEC      30

/*
 * Topik Publish Telemetri VMC:
 * Menggunakan "gh/GH-001/node/1/telemetry" agar otomatis diterima oleh
 * Dashboard sekaligus diteruskan oleh ESP32-C3 Node Kontrol ke STM32F401.
 */
#define TOPIC_PUB_VMC_TELEMETRY "gh/GH-001/node/1/telemetry"

/* ================= 2. KONFIGURASI PIN HARDWARE ====================== */
#define LED_STATUS_PIN          8    // LED bawaan ESP32-C3 (Active LOW)
#define STM8_UART_RX_PIN        9    // Terhubung ke PC6 STM8 (Soft-UART TX)
#define STM8_UART_TX_PIN        10   // Terhubung ke PC7 STM8 (Soft-UART RX)
#define STM8_UART_BAUD          9600 // Sesuai bit-timing TIM4 STM8

#endif /* CONFIG_H */