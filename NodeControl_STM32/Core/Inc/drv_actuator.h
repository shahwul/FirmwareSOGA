/*
 * drv_actuator.h
 *
 *  Created on: Oct 14, 2026
 *      Author: lenov
 */

#ifndef INC_DRV_ACTUATOR_H_
#define INC_DRV_ACTUATOR_H_

#ifndef DRV_ACTUATOR_H
#define DRV_ACTUATOR_H

#include "soga_config.h"

typedef struct {
    uint8_t rly1_air;       // PB12 - Valve Air Baku
    uint8_t rly2_nut_a;     // PB13 - Valve Nutrisi A
    uint8_t rly3_nut_b;     // PB14 - Valve Nutrisi B
    uint8_t rly4_irigasi;   // PB15 - Valve Distribusi Irigasi
    uint8_t rly5_mix_loop;  // PA8  - Valve Sirkulasi Mixing
    uint8_t rly6_pump;      // PB5  - Relay Pompa Utama
} SogaActuator_t;

extern SogaActuator_t g_Actuators;

void Actuator_Init(void);
void Actuator_ApplyState(const SogaActuator_t *state);
void Actuator_StopAll(void);

void Actuator_SetInletValve(uint8_t air, uint8_t nut_a, uint8_t nut_b);
void Actuator_StartMixingCirculation(void);
void Actuator_StartIrrigationPump(void);
void Actuator_StopPumpSafe(void);
void Actuator_SetRGB(uint8_t r, uint8_t g, uint8_t b);

#endif /* DRV_ACTUATOR_H */

#endif /* INC_DRV_ACTUATOR_H_ */
