#include "stm8_parser.h"
#include "config.h"

static HardwareSerial SerialSTM8(1);
static char   s_lineBuf[160];
static size_t s_lineIdx = 0;
static uint32_t s_validCount = 0;
static uint32_t s_errorCount = 0;

void STM8Parser_Init(void) {
    SerialSTM8.setRxBufferSize(256);
    SerialSTM8.begin(STM8_UART_BAUD, SERIAL_8N1, STM8_UART_RX_PIN, STM8_UART_TX_PIN);
}

/* Memverifikasi XOR Checksum pada frame: $VMC,v1,v2,v3,v4,v5,r1,r2,r3,r4,r5*CS */
static bool VerifyFrameChecksum(const char *line) {
    if (line[0] != '$') return false;

    const char *star = strchr(line, '*');
    if (star == NULL || strlen(star) < 3) return false;

    uint8_t calcXor = 0;
    for (const char *p = line + 1; p < star; p++) {
        calcXor ^= (uint8_t)(*p);
    }

    uint8_t recvXor = (uint8_t)strtol(star + 1, NULL, 16);
    return (calcXor == recvXor);
}

bool STM8Parser_Poll(VmcTelemetry_t *outData) {
    while (SerialSTM8.available()) {
        char c = (char)SerialSTM8.read();
        if (c == '\r') continue;

        if (c == '\n') {
            s_lineBuf[s_lineIdx] = '\0';
            s_lineIdx = 0;

            if (strncmp(s_lineBuf, "$VMC,", 5) != 0) {
                continue;
            }

            if (!VerifyFrameChecksum(s_lineBuf)) {
                s_errorCount++;
                Serial.printf("[STM8 RX] Checksum Error! Frame di-drop: %s\n", s_lineBuf);
                continue;
            }

            unsigned int v[5] = {0};
            unsigned int r[5] = {0};
            int parsed = sscanf(s_lineBuf, "$VMC,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u*",
                                &v[0], &v[1], &v[2], &v[3], &v[4],
                                &r[0], &r[1], &r[2], &r[3], &r[4]);

            if (parsed == 10) {
                float sum = 0.0f;
                for (int i = 0; i < 5; i++) {
                    outData->moisture_pct[i] = (float)v[i] / 10.0f;
                    outData->raw_adc[i]      = (uint16_t)r[i];
                    sum += outData->moisture_pct[i];
                }
                outData->moisture_avg = sum / 5.0f;
                outData->valid_frames = ++s_validCount;
                outData->crc_errors   = s_errorCount;
                return true;
            }
        } else {
            if (s_lineIdx < (sizeof(s_lineBuf) - 1)) {
                s_lineBuf[s_lineIdx++] = c;
            } else {
                s_lineIdx = 0;
            }
        }
    }
    return false;
}

void STM8Parser_BuildJSON(const VmcTelemetry_t *data, char *outJson, size_t maxLen) {
    /*
     * Kunci "soil_moisture":[v1,v2,v3,v4,v5] sengaja disamakan persis dengan
     * parser di STM32F401 Node Kontrol (app_rtos.c).
     */
    snprintf(outJson, maxLen,
             "{\"type\":\"telemetry\",\"node\":\"%s\","
             "\"soil_moisture\":[%.1f,%.1f,%.1f,%.1f,%.1f],"
             "\"vmc_avg\":%.1f,"
             "\"raw_adc\":[%u,%u,%u,%u,%u],"
             "\"seq\":%lu,\"crc_err\":%lu}",
             NODE_ID,
             data->moisture_pct[0], data->moisture_pct[1],
             data->moisture_pct[2], data->moisture_pct[3], data->moisture_pct[4],
             data->moisture_avg,
             data->raw_adc[0], data->raw_adc[1],
             data->raw_adc[2], data->raw_adc[3], data->raw_adc[4],
             (unsigned long)data->valid_frames,
             (unsigned long)data->crc_errors);
}