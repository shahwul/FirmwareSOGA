#ifndef STM32_BRIDGE_H
#define STM32_BRIDGE_H

#include <Arduino.h>

void STM32Bridge_Init(void);

// Mengirim baris JSON ke STM32 dengan aman (dilindungi Mutex)
bool STM32Bridge_SendLine(const char *jsonLine);

// Membaca baris JSON yang masuk dari STM32 (non-blocking)
// Mengembalikan true jika satu baris utuh ('\n') selesai dibaca
bool STM32Bridge_ReadLine(char *outBuffer, size_t maxLen);

#endif /* STM32_BRIDGE_H */