#ifndef DRV_ADC_VMC_H
#define DRV_ADC_VMC_H

#include "stm8_board.h"

void VMC_ADC_Init(void);
void VMC_ADC_SampleAll(uint16_t *out_vmc_decideg, uint16_t *out_adc_raw);

#endif /* DRV_ADC_VMC_H */