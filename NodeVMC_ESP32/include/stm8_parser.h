#ifndef STM8_PARSER_H
#define STM8_PARSER_H

#include <Arduino.h>

typedef struct {
    float    moisture_pct[5]; // Nilai 0.0% .. 100.0% untuk VMC1..VMC5
    float    moisture_avg;    // Rata-rata 5 sensor VMC (%)
    uint16_t raw_adc[5];      // Nilai ADC 10-bit mentah (0..1023)
    uint32_t valid_frames;
    uint32_t crc_errors;
} VmcTelemetry_t;

void STM8Parser_Init(void);
bool STM8Parser_Poll(VmcTelemetry_t *outData);
void STM8Parser_BuildJSON(const VmcTelemetry_t *data, char *outJson, size_t maxLen);

#endif /* STM8_PARSER_H */