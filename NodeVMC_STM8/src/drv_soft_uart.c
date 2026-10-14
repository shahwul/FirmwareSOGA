#include "drv_soft_uart.h"

static uint8_t s_runningXor = 0;

void SoftUART_Init(void) {
    /*
     * PENTING: Set PC6 (TX) ke HIGH terlebih dahulu sebelum mengaktifkan DDR Output
     * agar GPIO9 pada ESP32-C3 tidak tertarik ke LOW saat proses booting.
     */
    PC_ODR |= PIN_SUART_TX;
    PC_DDR |= PIN_SUART_TX;  // PC6 sebagai Output
    PC_CR1 |= PIN_SUART_TX;  // Push-Pull
    PC_CR2 |= PIN_SUART_TX;  // Fast slope

    /* Konfigurasi PC7 (RX) sebagai Input dengan Pull-Up internal */
    PC_DDR &= (uint8_t)(~PIN_SUART_RX);
    PC_CR1 |= PIN_SUART_RX;  // Pull-up aktif
    PC_CR2 &= (uint8_t)(~PIN_SUART_RX);

    /* Konfigurasi TIM4: F_CPU = 16 MHz, Prescaler = 16 (2^4) -> 1 tick = 1 us */
    TIM4_CR1  = 0x00;
    TIM4_PSCR = 0x04;        // 16 MHz / 16 = 1 MHz (1 us per tick)
    TIM4_ARR  = 0xFF;
    TIM4_EGR  = 0x01;        // Generate update event untuk memuat prescaler
    TIM4_CR1  = 0x01;        // Jalankan TIM4
}

void SoftUART_DelayUs(uint8_t us) {
    TIM4_CNTR = 0;
    while (TIM4_CNTR < us);
}

void SoftUART_DelayMs(uint16_t ms) {
    while (ms--) {
        SoftUART_DelayUs(250);
        SoftUART_DelayUs(250);
        SoftUART_DelayUs(250);
        SoftUART_DelayUs(250);
    }
}

void SoftUART_SendByte(uint8_t data) {
    uint8_t i;
    DISABLE_INTERRUPTS();

    /* 1. Start Bit (LOW) */
    PC_ODR &= (uint8_t)(~PIN_SUART_TX);
    SoftUART_DelayUs(SUART_BIT_US);

    /* 2. 8 Data Bits (LSB First) */
    for (i = 0; i < 8; i++) {
        if (data & 0x01) {
            PC_ODR |= PIN_SUART_TX;
        } else {
            PC_ODR &= (uint8_t)(~PIN_SUART_TX);
        }
        data >>= 1;
        SoftUART_DelayUs(SUART_BIT_US);
    }

    /* 3. Stop Bit (HIGH) */
    PC_ODR |= PIN_SUART_TX;
    SoftUART_DelayUs(SUART_BIT_US);

    ENABLE_INTERRUPTS();
}

static void SendCharChecksum(char c) {
    s_runningXor ^= (uint8_t)c;
    SoftUART_SendByte((uint8_t)c);
}

static void SendUIntChecksum(uint16_t val) {
    char buf[6];
    uint8_t i = 0;

    if (val == 0) {
        SendCharChecksum('0');
        return;
    }
    while (val > 0 && i < 5) {
        buf[i++] = (char)('0' + (val % 10U));
        val /= 10U;
    }
    while (i > 0) {
        SendCharChecksum(buf[--i]);
    }
}

static void SendHexByte(uint8_t b) {
    const char hexLut[] = "0123456789ABCDEF";
    SoftUART_SendByte((uint8_t)hexLut[(b >> 4) & 0x0F]);
    SoftUART_SendByte((uint8_t)hexLut[b & 0x0F]);
}

void SoftUART_SendString(const char *str) {
    while (*str) {
        SoftUART_SendByte((uint8_t)(*str++));
    }
}

void SoftUART_SendUInt(uint16_t val) {
    s_runningXor = 0;
    SendUIntChecksum(val);
}

void SoftUART_SendVmcFrame(const uint16_t *vmc_decideg, const uint16_t *adc_raw) {
    uint8_t i;
    const char *header = "VMC";

    SoftUART_SendByte('$');
    s_runningXor = 0; // Checksum dihitung untuk seluruh karakter di antara '$' dan '*'

    while (*header) {
        SendCharChecksum(*header++);
    }

    /* Kirim 5 nilai VMC (0..1000 = 0.0%..100.0%) */
    for (i = 0; i < VMC_CHANNEL_COUNT; i++) {
        SendCharChecksum(',');
        SendUIntChecksum(vmc_decideg[i]);
    }

    /* Kirim 5 nilai ADC mentah (0..1023) untuk keperluan diagnostik/kalibrasi */
    for (i = 0; i < VMC_CHANNEL_COUNT; i++) {
        SendCharChecksum(',');
        SendUIntChecksum(adc_raw[i]);
    }

    /* Akhiri dengan *CS\r\n */
    SoftUART_SendByte('*');
    SendHexByte(s_runningXor);
    SoftUART_SendByte('\r');
    SoftUART_SendByte('\n');
}