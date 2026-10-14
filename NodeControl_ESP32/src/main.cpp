#include <Arduino.h>
#include "config.h"
#include "stm32_bridge.h"
#include "net_manager.h"

static TaskHandle_t s_taskNetHandle  = NULL;
static TaskHandle_t s_taskUartHandle = NULL;
static TaskHandle_t s_taskNtpHandle  = NULL;

/* ================= TASK 1: NETWORK & MQTT MANAGER ================= */
static void TaskNetwork(void *pvParameters) {
    (void)pvParameters;
    Serial.printf("[RTOS] TaskNetwork aktif di Core %d\n", xPortGetCoreID());

    for (;;) {
        NetManager_Step();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ================= TASK 2: UART RX ROUTER (STM32 -> MQTT) ========= */
static void TaskUARTRX(void *pvParameters) {
    (void)pvParameters;
    Serial.printf("[RTOS] TaskUARTRX aktif di Core %d\n", xPortGetCoreID());

    char rxLine[MQTT_BUFFER_SIZE];

    for (;;) {
        while (STM32Bridge_ReadLine(rxLine, sizeof(rxLine))) {
            Serial.printf("\n[UART RX] Laporan dari STM32: %s\n", rxLine);
            NetManager_RouteUpstreamJSON(rxLine);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ================= TASK 3: NTP TIME INJECTOR (ESP32 -> STM32) ===== */
static void TaskTimeSync(void *pvParameters) {
    (void)pvParameters;
    Serial.println("[RTOS] TaskTimeSync aktif!");

    // Beri waktu 10 detik saat booting agar WiFi tersambung lebih dulu
    vTaskDelay(pdMS_TO_TICKS(10000));

    for (;;) {
        NetManager_SyncTimeToSTM32();
        vTaskDelay(pdMS_TO_TICKS(NTP_SYNC_INTERVAL_MS));
    }
}

/* ================= SETUP & LOOP =================================== */
void setup() {
    delay(2000);
    Serial.begin(115200);
    delay(500);

    Serial.println("\n==============================================");
    Serial.println("   SOGA FARM - CONTROL GATEWAY (ESP32-C3)     ");
    Serial.println("==============================================");

    STM32Bridge_Init();
    NetManager_Init();

    // ESP32-C3 adalah Single-Core (Core 0), semua task di-pin ke Core 0
    xTaskCreatePinnedToCore(TaskNetwork,  "Network_Task",  4096, NULL, 2, &s_taskNetHandle,  0);
    xTaskCreatePinnedToCore(TaskUARTRX,   "UART_RX_Task",  4096, NULL, 2, &s_taskUartHandle, 0);
    xTaskCreatePinnedToCore(TaskTimeSync, "TimeSync_Task", 4096, NULL, 1, &s_taskNtpHandle,  0);
}

void loop() {
    vTaskDelete(NULL); // Hapus loop bawaan Arduino agar seluruh CPU dikelola FreeRTOS
}