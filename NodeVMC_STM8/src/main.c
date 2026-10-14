#include "stm8_board.h"
#include "drv_soft_uart.h"
#include "drv_adc_vmc.h"


static void SystemClock_16MHz_Init(void) {
    CLK_CKDIVR  = 0x00; // HSI 16 MHz tanpa pembagi (Prescaler = 1)
    CLK_PCKENR1 = 0xFF; // Aktifkan clock TIM4
    CLK_PCKENR2 = 0xFF; // Aktifkan clock ADC1
}

static void RGB_LED_Init(void) {
    /* PC3 (LED_R) dan PC5 (LED_G) sebagai Output Push-Pull */
    PC_ODR &= (uint8_t)(~(PIN_LED_R | PIN_LED_G));
    PC_DDR |= (PIN_LED_R | PIN_LED_G);
    PC_CR1 |= (PIN_LED_R | PIN_LED_G);

    /* PD4 (LED_B) sebagai Output Push-Pull */
    PD_ODR &= (uint8_t)(~PIN_LED_B);
    PD_DDR |= PIN_LED_B;
    PD_CR1 |= PIN_LED_B;
}

static void RGB_LED_Set(bool r, bool g, bool b) {
    if (r) PC_ODR |= PIN_LED_R; else PC_ODR &= (uint8_t)(~PIN_LED_R);
    if (g) PC_ODR |= PIN_LED_G; else PC_ODR &= (uint8_t)(~PIN_LED_G);
    if (b) PD_ODR |= PIN_LED_B; else PD_ODR &= (uint8_t)(~PIN_LED_B);
}

void main(void) {
    uint16_t vmc_decideg[VMC_CHANNEL_COUNT];
    uint16_t adc_raw[VMC_CHANNEL_COUNT];

    SystemClock_16MHz_Init();
    SoftUART_Init(); // PC6 langsung HIGH agar GPIO9 ESP32-C3 aman saat boot
    RGB_LED_Init();
    VMC_ADC_Init();

    /* Tunggu 1 detik saat cold-boot agar ESP32-C3 selesai masuk mode aplikasi */
    RGB_LED_Set(0, 1, 0); // Hijau: Inisialisasi sukses
    SoftUART_DelayMs(1000);

    while (1) {
        /* 1. Kedipkan LED Biru saat mengakuisisi 5 sensor & mengirim UART */
        RGB_LED_Set(0, 0, 1);
        VMC_ADC_SampleAll(vmc_decideg, adc_raw);
        SoftUART_SendVmcFrame(vmc_decideg, adc_raw);

        /* 2. Kembali ke LED Hijau (Standby) */
        SoftUART_DelayMs(80);
        RGB_LED_Set(0, 1, 0);

        /* 3. Tunggu sisa periode (total siklus ~2 detik) */
        SoftUART_DelayMs(PUBLISH_PERIOD_MS - 80U);
    }
}