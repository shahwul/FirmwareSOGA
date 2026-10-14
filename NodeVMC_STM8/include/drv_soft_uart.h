#ifndef DRV_SOFT_UART_H
#define DRV_SOFT_UART_H

#include "stm8_board.h"

void SoftUART_Init(void);
void SoftUART_DelayUs(uint8_t us);
void SoftUART_DelayMs(uint16_t ms);

void SoftUART_SendByte(uint8_t data);
void SoftUART_SendString(const char *str);
void SoftUART_SendUInt(uint16_t val);

/*
 * Mengirim paket lengkap 5 sensor VMC ke ESP32-C3 dengan XOR Checksum:
 * Format: $VMC,v1,v2,v3,v4,v5,raw1,raw2,raw3,raw4,raw5*CS\r\n
 * (v1..v5 bernilai 0..1000 yang merepresentasikan 0.0%..100.0%)
 */
void SoftUART_SendVmcFrame(const uint16_t *vmc_decideg, const uint16_t *adc_raw);

#endif /* DRV_SOFT_UART_H */