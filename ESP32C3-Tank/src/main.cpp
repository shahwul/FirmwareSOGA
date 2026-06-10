#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

// ================= CONFIG WIFI & MQTT =================
const char *ssid = "tempe kanget";
const char *password = "12345678";

const uint8_t router_bssid[] = {0x5C, 0x02, 0x14, 0xED, 0xAA, 0x98}; 

const char *mqtt_server = "10.161.28.83";
const int mqtt_port = 1883;

const char* topic_sub_monitor = "gh/GH-001/node/1/telemetry";
const char* topic_sub_perintah = "gh/GH-001/command/water/2"; 

const char* topic_pub_ack = "gh/GH-001/command/water/2/ack";
const char* topic_pub_telemetry = "gh/GH-001/node/2/telemetry";

String node_id = "Soga_Tank_Node"; 

// ================= CONFIG NTP (WAKTU WIB) =================
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600; // Offset WIB (UTC +7 Jam)
const int   daylightOffset_sec = 0;

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
TaskHandle_t TaskTimeSync_Handle; // Ganti nama biar sesuai tugasnya

// ================= FUNGSI CALLBACK MQTT =================
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String incomingMessage = "";
  for (int i = 0; i < length; i++) {
    incomingMessage += (char)payload[i];
  }
  
  Serial.println("\n[MQTT SUB] Masuk data baru dari broker: " + incomingMessage);
  SerialSTM32.println(incomingMessage);
  Serial.println("[FORWARDER] Data sukses diteruskan ke STM32!");
}

// ================= TASK UART (ROUTER JSON) =================
void TaskUARTRX(void *pvParameters) {
  Serial.print("[RTOS] Task Upstream UART aktif di Core: ");
  Serial.println(xPortGetCoreID());
  
  String rxBuffer = "";

  for (;;) {
    while (SerialSTM32.available()) {
      char c = SerialSTM32.read();
      rxBuffer += c;

      if (c == '\n') {
        rxBuffer.trim(); 

        if (rxBuffer.length() > 0 && rxBuffer.startsWith("{")) {
          Serial.println("\n[UART RX] Terima laporan STM32: " + rxBuffer);
          
          if (client.connected()) {
            // 1. JALUR ACK
            if (rxBuffer.indexOf("\"type\":\"ack\"") > 0 || rxBuffer.indexOf("\"siram_selesai\"") > 0) {
                client.publish(topic_pub_ack, rxBuffer.c_str());
            }
            // 2. JALUR CONTROL STATE
            else if (rxBuffer.indexOf("\"type\":\"control_state\"") > 0) {
                client.publish(topic_pub_telemetry, rxBuffer.c_str());
            }
            // 3. JALUR HEARTBEAT / TELEMETRI
            else if (rxBuffer.indexOf("\"type\":\"heartbeat\"") > 0 || rxBuffer.indexOf("\"uptime\"") > 0) {
                client.publish(topic_pub_telemetry, rxBuffer.c_str());
            }
            else {
                Serial.println("[WARNING] Format JSON tidak dikenali, drop!");
            }
          } else {
            Serial.println("[MQTT PUB] Gagal lapor, Broker putus!");
          }
        }
        rxBuffer = ""; 
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ================= TASK NTP TIME INJECTOR =================
void TaskTimeSync(void *pvParameters) {
  Serial.println("[RTOS] Task NTP Time Sync Aktif!");
  
  // Tunggu sebentar sampai TaskNetwork selesai nyambung WiFi pertama kali
  vTaskDelay(pdMS_TO_TICKS(10000)); 

  for (;;) {
    struct tm timeinfo;
    
    // Cek apakah WiFi nyala dan berhasil dapat jam dari NTP
    if (WiFi.status() == WL_CONNECTED && getLocalTime(&timeinfo)) {
      
      // Bikin payload JSON berisi jam, menit, detik (Suntik Waktu)
      String payloadTime = "{\"cmd\":\"sync_rtc\",\"h\":" + String(timeinfo.tm_hour) + 
                           ",\"m\":" + String(timeinfo.tm_min) + 
                           ",\"s\":" + String(timeinfo.tm_sec) + "}";
                           
      SerialSTM32.println(payloadTime);
      
      Serial.println("\n==================================================");
      Serial.printf("[NTP SYNC] Waktu sekarang: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      Serial.println("[NTP SYNC] Suntik waktu ke STM32: " + payloadTime);
      Serial.println("==================================================");
      
    } else {
      Serial.println("[NTP SYNC] Warning: Gagal sinkron NTP (WiFi mati / Server Down)");
    }

    // Suntik waktu ke STM32 setiap 1 menit sekali biar akurat terus
    vTaskDelay(pdMS_TO_TICKS(60000)); 
  }
}

// ================= TASK NETWORK MANAGER =================
void TaskNetwork(void *pvParameters) {
  Serial.print("[RTOS] Task Network berjalan di Core: ");
  Serial.println(xPortGetCoreID());

  WiFi.setAutoReconnect(true);
  int mqtt_fail_count = 0; 

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\n[NETWORK] WiFi Putus! Mencari jaringan...");
      WiFi.disconnect(true); 
      vTaskDelay(pdMS_TO_TICKS(500)); 
      
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
      }
    }
    else { 
      if (!client.connected()) {
        if (client.connect(node_id.c_str())) {
          client.subscribe(topic_sub_monitor);
          client.subscribe(topic_sub_perintah);
          mqtt_fail_count = 0; 
        } else {
          mqtt_fail_count++;
          if (mqtt_fail_count >= 3) {
            WiFi.disconnect(); 
            vTaskDelay(pdMS_TO_TICKS(1000));
            mqtt_fail_count = 0;
          }
        }
      } else {
        client.loop(); 
        digitalWrite(LED_PIN, (millis() / 1000) % 2 == 0 ? LOW : HIGH);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50)); 
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
  Serial.println("   SOGA FARM - GATEWAY NODE (NTP SYNC)        ");
  Serial.println("==============================================");
  
  SerialSTM32.begin(115200, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); 
  
  // INIT NTP SERVER (Biar ESP32 otomatis nge-fetch di background)
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqtt_callback); 
  client.setKeepAlive(30);

  // START RTOS TASKS
  xTaskCreatePinnedToCore(TaskNetwork, "Network_Task", 4096, NULL, 2, &TaskNetwork_Handle, 1);
  xTaskCreatePinnedToCore(TaskUARTRX, "UART_RX_Task", 4096, NULL, 2, &TaskUART_Handle, 1);
  xTaskCreatePinnedToCore(TaskTimeSync, "TimeSync_Task", 4096, NULL, 1, &TaskTimeSync_Handle, 0);
}

void loop() {
  vTaskDelete(NULL); 
}