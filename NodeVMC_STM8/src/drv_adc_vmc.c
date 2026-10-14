#include "drv_adc_vmc.h"
#include "drv_soft_uart.h"

static const uint8_t s_adcChannels[VMC_CHANNEL_COUNT] = {
    ADC_CH_VMC1, // AIN2 - PC4 (Pin 14)
    ADC_CH_VMC2, // AIN3 - PD2 (Pin 19)
    ADC_CH_VMC3, // AIN4 - PD3 (Pin 20)
    ADC_CH_VMC4, // AIN5 - PD5 (Pin 2)
    ADC_CH_VMC5  // AIN6 - PD6 (Pin 3)
};

static uint16_t s_filteredRaw[VMC_CHANNEL_COUNT] = {0};
static bool     s_firstRun = true;

void VMC_ADC_Init(void) {
    /*
     * 1. Set pin PC4, PD2, PD3, PD5, PD6 sebagai Floating Input (Tanpa Pull-Up)
     */
    PC_DDR &= (uint8_t)(~(1U << 4));
    PC_CR1 &= (uint8_t)(~(1U << 4));
    PC_CR2 &= (uint8_t)(~(1U << 4));

    PD_DDR &= (uint8_t)(~((1U << 2) | (1U << 3) | (1U << 5) | (1U << 6)));
    PD_CR1 &= (uint8_t)(~((1U << 2) | (1U << 3) | (1U << 5) | (1U << 6)));
    PD_CR2 &= (uint8_t)(~((1U << 2) | (1U << 3) | (1U << 5) | (1U << 6)));

    /*
     * 2. Matikan Schmitt Trigger pada AIN2..AIN6 (ADC_TDRL bit 2..6)
     *    untuk menghilangkan arus bocor I/O digital dan meningkatkan akurasi ADC.
     */
    ADC_TDRL |= (uint8_t)((1U << 2) | (1U << 3) | (1U << 4) | (1U << 5) | (1U << 6));

    /*
     * 3. Konfigurasi ADC1:
     *    - Right alignment (ADC_CR2 bit 3 = 1)
     *    - Prescaler f_ADC = f_MASTER / 8 = 2 MHz (ADC_CR1 bits [6:4] = 0b100)
     */
    ADC_CR2 = (1U << 3);
    ADC_CR1 = (0x40U) | 0x01U; // Aktifkan ADC (ADON = 1)
    SoftUART_DelayUs(20);      // Waktu stabilisasi internal ADC (> 7 us)
}

static uint16_t ReadSingleChannelOversampled(uint8_t ch) {
    uint32_t sum = 0;
    uint8_t i;

    /* Pilih kanal ADC (bit 0..3 pada ADC_CSR) dan bersihkan bendera EOC (bit 7) */
    ADC_CSR = (uint8_t)(ch & 0x0FU);

    /* Buang 1 pembacaan pertama setelah perpindahan kanal (settling kapasitor S/H) */
    ADC_CR1 |= 0x01U;
    while ((ADC_CSR & 0x80U) == 0);
    ADC_CSR &= (uint8_t)(~0x80U);
    (void)ADC_DRL;
    (void)ADC_DRH;

    /* Ambil rata-rata 16 sampel berturut-turut (16x Oversampling) */
    for (i = 0; i < 16U; i++) {
        ADC_CR1 |= 0x01U; // Mulai konversi
        while ((ADC_CSR & 0x80U) == 0);
        ADC_CSR &= (uint8_t)(~0x80U);

        /* Pada mode Right-Aligned, DRL WAJIB dibaca sebelum DRH */
        uint8_t low  = ADC_DRL;
        uint8_t high = ADC_DRH;
        sum += (uint16_t)(((uint16_t)high << 8) | low);
    }

    return (uint16_t)(sum >> 4); // Dibagi 16
}

static uint16_t ConvertRawToDeciPercent(uint16_t raw) {
    /*
     * Konversi terbalik untuk sensor kapasitif (Raw tinggi = Kering, Raw rendah = Basah)
     * Output: 0 .. 1000 (mewakili 0.0% .. 100.0%)
     */
    #if (VMC_ADC_DRY_RAW > VMC_ADC_WET_RAW)
    if (raw >= VMC_ADC_DRY_RAW) return 0U;
    if (raw <= VMC_ADC_WET_RAW) return 1000U;

    uint32_t num = (uint32_t)(VMC_ADC_DRY_RAW - raw) * 1000UL;
    uint32_t den = (uint32_t)(VMC_ADC_DRY_RAW - VMC_ADC_WET_RAW);
    return (uint16_t)(num / den);
    #else
    /* Jika menggunakan sensor tipe resistif/tegangan naik saat basah */
    if (raw <= VMC_ADC_DRY_RAW) return 0U;
    if (raw >= VMC_ADC_WET_RAW) return 1000U;

    uint32_t num = (uint32_t)(raw - VMC_ADC_DRY_RAW) * 1000UL;
    uint32_t den = (uint32_t)(VMC_ADC_WET_RAW - VMC_ADC_DRY_RAW);
    return (uint16_t)(num / den);
    #endif
}

void VMC_ADC_SampleAll(uint16_t *out_vmc_decideg, uint16_t *out_adc_raw) {
    uint8_t i;

    for (i = 0; i < VMC_CHANNEL_COUNT; i++) {
        uint16_t sample = ReadSingleChannelOversampled(s_adcChannels[i]);

        /* Exponential Moving Average (EMA): 75% nilai lama + 25% sampel baru */
        if (s_firstRun) {
            s_filteredRaw[i] = sample;
        } else {
            s_filteredRaw[i] = (uint16_t)(((uint32_t)s_filteredRaw[i] * 3UL + sample) >> 2);
        }

        out_adc_raw[i]     = s_filteredRaw[i];
        out_vmc_decideg[i] = ConvertRawToDeciPercent(s_filteredRaw[i]);
    }
    s_firstRun = false;
}