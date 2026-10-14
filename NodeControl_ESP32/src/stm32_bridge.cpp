#include "stm32_bridge.h"
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static HardwareSerial SerialSTM32(1);
static SemaphoreHandle_t s_uartTxMutex = NULL;

static char   s_rxBuf[MQTT_BUFFER_SIZE];
static size_t s_rxIdx = 0;

void STM32Bridge_Init(void) {
    s_uartTxMutex = xSemaphoreCreateMutex();
    SerialSTM32.setRxBufferSize(MQTT_BUFFER_SIZE);
    SerialSTM32.begin(STM32_UART_BAUD, SERIAL_8N1, STM32_UART_RX_PIN, STM32_UART_TX_PIN);
}

bool STM32Bridge_SendLine(const char *jsonLine) {
    if (s_uartTxMutex == NULL || jsonLine == NULL) return false;

    // Tunggu maksimal 100ms jika task lain sedang mengirim data ke STM32
    if (xSemaphoreTake(s_uartTxMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        SerialSTM32.println(jsonLine);
        xSemaphoreGive(s_uartTxMutex);
        return true;
    }
    return false;
}

bool STM32Bridge_ReadLine(char *outBuffer, size_t maxLen) {
    while (SerialSTM32.available()) {
        char c = (char)SerialSTM32.read();

        if (c == '\r') continue; // Abaikan carriage return

        if (c == '\n') {
            s_rxBuf[s_rxIdx] = '\0';
            s_rxIdx = 0;

            // Hanya proses jika diawali kurung kurawal JSON '{'
            if (s_rxBuf[0] == '{') {
                strncpy(outBuffer, s_rxBuf, maxLen - 1);
                outBuffer[maxLen - 1] = '\0';
                return true;
            }
        } else {
            if (s_rxIdx < ( sizeof(s_rxBuf) - 1 )) {
                s_rxBuf[s_rxIdx++] = c;
            } else {
                // Buffer overflow guard: reset jika ada data korup tanpa '\n'
                s_rxIdx = 0;
            }
        }
    }
    return false;
}