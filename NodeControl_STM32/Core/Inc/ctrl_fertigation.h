/*
 * ctrl_fertigation.h
 *
 *  Created on: Oct 14, 2026
 *      Author: lenov
 */

#ifndef INC_CTRL_FERTIGATION_H_
#define INC_CTRL_FERTIGATION_H_

#ifndef CTRL_FERTIGATION_H
#define CTRL_FERTIGATION_H

#include "soga_config.h"

typedef struct {
    float    current_vpd;
    float    current_moisture;
    float    dynamic_sp;
    float    recipe_air_L;
    float    recipe_nut_a_L;
    float    recipe_nut_b_L;
    float    recipe_out_L;
    uint8_t  is_auto_mode;
    uint8_t  flag_manual_air;
    uint8_t  flag_manual_nutrisi;
    uint8_t  latch_dosing_event;
    uint8_t  latch_nutrient_event;
    char     status_mesin[32];
} SogaSystemState_t;

extern SogaSystemState_t g_SysState;

void  Fertigation_Init(void);
float Fertigation_ComputeDynamicSP(float vpd_kpa);
bool  Fertigation_RunCycle(float liter_air, float liter_nut_a, float liter_nut_b, float liter_out_irigasi);

#endif /* CTRL_FERTIGATION_H */

#endif /* INC_CTRL_FERTIGATION_H_ */
