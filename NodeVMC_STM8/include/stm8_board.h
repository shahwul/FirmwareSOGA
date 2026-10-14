#ifndef STM8_BOARD_H
#define STM8_BOARD_H

#include <stdint.h>
#include <stdbool.h>

/* ================= 1. DEFINISI REGISTER STM8S103F3 ================== */
#define REG8(addr)              (*(volatile uint8_t *)(addr))

/* Clock (CLK) */
#define CLK_CKDIVR              REG8(0x50C6)
#define CLK_PCKENR1             REG8(0x50C7)
#define CLK_PCKENR2             REG8(0x50CA)

/* Port C (PC3..PC7) */
#define PC_ODR                  REG8(0x500A)
#define PC_IDR                  REG8(0x500B)
#define PC_DDR                  REG8(0x500C)
#define PC_CR1                  REG8(0x500D)
#define PC_CR2                  REG8(0x500E)

/* Port D (PD1..PD6) */
#define PD_ODR                  REG8(0x500F)
#define PD_IDR                  REG8(0x5010)
#define PD_DDR                  REG8(0x5011)
#define PD_CR1                  REG8(0x5012)
#define PD_CR2                  REG8(0x5013)

/* ADC1 (10-bit Successive Approximation) */
#define ADC_DB0RH               REG8(0x53E0)
#define ADC_CSR                 REG8(0x5400)
#define ADC_CR1                 REG8(0x5401)
#define ADC_CR2                 REG8(0x5402)
#define ADC_CR3                 REG8(0x5403)
#define ADC_DRH                 REG8(0x5404)
#define ADC_DRL                 REG8(0x5405)
#define ADC_TDRL                REG8(0x5407)

/* TIM4 (8-bit Basic Timer untuk Bit-Timing Soft-UART & Delay) */
#define TIM4_CR1                REG8(0x5340)
#define TIM4_IER                REG8(0x5343)
#define TIM4_SR                 REG8(0x5344)
#define TIM4_EGR                REG8(0x5345)
#define TIM4_CNTR               REG8(0x5346)
#define TIM4_PSCR               REG8(0x5347)
#define TIM4_ARR                REG8(0x5348)

/* Instruksi Assembly SDCC */
#define DISABLE_INTERRUPTS()    __asm__("sim")
#define ENABLE_INTERRUPTS()     __asm__("rim")
#define NOP()                   __asm__("nop")

/* ================= 2. PEMETAAN PIN FISIK NODE VMC =================== */
/*
 * Pin 13 (PC3) -> LED_R
 * Pin 15 (PC5) -> LED_G
 * Pin 1  (PD4) -> LED_B
 * Pin 16 (PC6) -> SOFT_UART_TX (Menuju RX GPIO9 ESP32-C3)
 * Pin 17 (PC7) -> SOFT_UART_RX (Menuju TX GPIO10 ESP32-C3)
 */
#define PIN_LED_R               (1U << 3) // PC3
#define PIN_LED_G               (1U << 5) // PC5
#define PIN_LED_B               (1U << 4) // PD4

#define PIN_SUART_TX            (1U << 6) // PC6
#define PIN_SUART_RX            (1U << 7) // PC7

/*
 * 5 Kanal Analog Input (ADC1):
 * VMC1 -> PC4 (Pin 14) = AIN2
 * VMC2 -> PD2 (Pin 19) = AIN3
 * VMC3 -> PD3 (Pin 20) = AIN4
 * VMC4 -> PD5 (Pin 2)  = AIN5
 * VMC5 -> PD6 (Pin 3)  = AIN6
 */
#define VMC_CHANNEL_COUNT       5U
#define ADC_CH_VMC1             2U // AIN2 (PC4)
#define ADC_CH_VMC2             3U // AIN3 (PD2)
#define ADC_CH_VMC3             4U // AIN4 (PD3)
#define ADC_CH_VMC4             5U // AIN5 (PD5)
#define ADC_CH_VMC5             6U // AIN6 (PD6)

/* ================= 3. KALIBRASI SENSOR VMC (10-BIT: 0 - 1023) ======= */
/*
 * Catatan Kalibrasi Sensor Kapasitif Soil Moisture (3.3V):
 * - Saat kering di udara terbuka (0.0%), tegangan output tinggi (~2.6V -> ADC ~820)
 * - Saat tercelup air penuh (100.0%), tegangan output rendah (~1.2V -> ADC ~380)
 * Sesuaikan nilai di bawah dengan hasil pembacaan raw sensor milikmu.
 */
#define VMC_ADC_DRY_RAW         820U  // Nilai ADC saat tanah kering (0.0%)
#define VMC_ADC_WET_RAW         380U  // Nilai ADC saat tanah jenuh air (100.0%)

/* ================= 4. PARAMETER KOMUNIKASI & TIMING ================= */
#define SUART_BIT_US            104U  // 1000000 / 9600 bps = 104.16 us per bit
#define PUBLISH_PERIOD_MS       2000U // Kirim data ke ESP32-C3 setiap 2 detik

#endif /* STM8_BOARD_H */