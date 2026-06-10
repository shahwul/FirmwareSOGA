#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ================= CONFIG WIFI & MQTT =================
const char *ssid = "tempe kanget";
const char *password = "12345678";

// BSSID Lock (Biar ESP32 gak bingung kalau ada WiFi extender bernama sama)
const uint8_t router_bssid[] = {0x5C, 0x02, 0x14, 0xED, 0xAA, 0x98}; 

const char *mqtt_server = "10.161.28.83"; 
const int mqtt_port = 1883; 

const char* topic_publish = "gh/GH-001/node/1/telemetry";

String node_id = "Soga_Monitor_Node";

// ================= HARDWARE CONFIG =================
const int LED_PIN = 8;

#define STM32_RX_PIN 9
#define STM32_TX_PIN 10
HardwareSerial SerialSTM32(1); 

WiFiClient espClient;
PubSubClient client(espClient);

// ================= DEKLARASI TASK RTOS =================
TaskHandle_t TaskNetwork_Handle;
TaskHandle_t TaskUART_Handle;

// ================= FUNGSI RTOS 1: NETWORK MANAGER =================
void TaskNetwork(void *pvParameters) {
  Serial.print("[RTOS] Task Network berjalan di Core: ");
  Serial.println(xPortGetCoreID());

  WiFi.setAutoReconnect(true);
  int mqtt_fail_count = 0; 

  for (;;) {
    // 1. CEK WIFI
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\n[NETWORK] WiFi Putus! Mencari jaringan...");
      WiFi.disconnect(true); 
      vTaskDelay(pdMS_TO_TICKS(500)); 
      
      WiFi.scanNetworks(false, true); 
      vTaskDelay(pdMS_TO_TICKS(100));
      
      WiFi.begin(ssid, password);
      
      int retry_wifi = 0;
      while (WiFi.status() != WL_CONNECTED && retry_wifi < 15) {
        digitalWrite(LED_PIN, LOW); 
        vTaskDelay(pdMS_TO_TICKS(200));
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(500));
        retry_wifi++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[NETWORK] WiFi SUKSES Connected! IP: " + WiFi.localIP().toString());
        mqtt_fail_count = 0; 
      } else {
        Serial.println("\n[NETWORK] Gagal masuk WiFi, istirahat sejenak...");
        vTaskDelay(pdMS_TO_TICKS(2000)); 
      }
    }

    // 2. CEK MQTT
    else { 
      if (!client.connected()) {
        Serial.println("[NETWORK] MQTT Putus! Nyambung ke broker publik...");
        
        if (client.connect(node_id.c_str())) {
          Serial.println("[NETWORK] MQTT SUKSES Connected ke Public Port!");
          mqtt_fail_count = 0; 
        } else {
          Serial.print("[NETWORK] MQTT Gagal, rc=");
          Serial.println(client.state());
          
          mqtt_fail_count++;
          if (mqtt_fail_count >= 3) {
            Serial.println("\n[FATAL] Fix ini WiFi Zombie/Nyantol! Reset WiFi...");
            WiFi.disconnect(); 
            vTaskDelay(pdMS_TO_TICKS(1000));
            mqtt_fail_count = 0;
          } else {
            vTaskDelay(pdMS_TO_TICKS(3000)); 
          }
        }
      } 
      else {
        client.loop(); 
        mqtt_fail_count = 0; 
        digitalWrite(LED_PIN, (millis() / 1000) % 2 == 0 ? LOW : HIGH);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50)); 
  }
}

// ================= FUNGSI RTOS 2: UART READER =================
void TaskUART(void *pvParameters) {
  Serial.print("[RTOS] Task UART berjalan di Core: ");
  Serial.println(xPortGetCoreID());
  
  String jsonBuffer = "";

  for (;;) {
    while (SerialSTM32.available()) {
      char c = SerialSTM32.read();
      jsonBuffer += c;

      if (c == '\n') {
        jsonBuffer.trim(); 
        
        if (jsonBuffer.length() > 0) {
          Serial.println("\n[UART] Terima data: " + jsonBuffer);

          if (client.connected()) {
            // Murni langsung tembak tanpa basa-basi hitung waktu
            if (client.publish(topic_publish, jsonBuffer.c_str())) {
              Serial.println("[MQTT] Sukses Publish ke Broker!");
            } else {
              Serial.println("[MQTT] Gagal Publish! Jalur publik macet.");
            }
          } else {
            Serial.println("[WARNING] Data dibuang, Broker Publik lagi putus!");
          }
        }
        jsonBuffer = ""; // Reset buffer
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20)); 
  }
}

// ================= SETUP =================
void setup() {
  delay(3000); 
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); 
  
  Serial.println("\n==============================================");
  Serial.println("   SOGA FARM - ESP32-C3 FREERTOS PUBLIC NODE  ");
  Serial.println("==============================================");
  
  SerialSTM32.begin(115200, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  client.setServer(mqtt_server, mqtt_port);
  client.setKeepAlive(30);

  xTaskCreate(TaskNetwork, "Network_Task", 4096, NULL, 1, &TaskNetwork_Handle);
  xTaskCreate(TaskUART, "UART_Task", 4096, NULL, 2, &TaskUART_Handle);
}

// ================= LOOP =================
void loop() {
  vTaskDelete(NULL); 
}