/*
 * ctrl_fertigation.c
 *
 *  Created on: Oct 14, 2026
 *      Author: lenov
 */

#include "ctrl_fertigation.h"
#include "drv_actuator.h"
#include "drv_sensors.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

SogaSystemState_t g_SysState = {
    .current_vpd          = 1.0f,
    .current_moisture     = 100.0f,
    .dynamic_sp           = SP_BASE_PCT,
    .recipe_air_L         = DEFAULT_RECIPE_AIR_L,
    .recipe_nut_a_L       = DEFAULT_RECIPE_NUT_A_L,
    .recipe_nut_b_L       = DEFAULT_RECIPE_NUT_B_L,
    .recipe_out_L         = DEFAULT_RECIPE_OUT_L,
    .is_auto_mode         = 0,
    .flag_manual_air      = 0,
    .flag_manual_nutrisi  = 0,
    .latch_dosing_event   = 0,
    .latch_nutrient_event = 0,
    .status_mesin         = "STANDBY"
};

void Fertigation_Init(void) {
    strcpy(g_SysState.status_mesin, "STANDBY");
}

float Fertigation_ComputeDynamicSP(float vpd_kpa) {
    float sp = SP_BASE_PCT + (SP_VPD_GAIN * vpd_kpa);
    if (sp < SP_MIN_PCT) sp = SP_MIN_PCT;
    if (sp > SP_MAX_PCT) sp = SP_MAX_PCT;
    return sp;
}

bool Fertigation_RunCycle(float liter_air, float liter_nut_a, float liter_nut_b, float liter_out_irigasi) {
    Actuator_SetRGB(0, 0, 1); // Biru: Sedang beroperasi

    /* 1. Takar Air Baku (RLY1) melewati FLOW1 */
    if (liter_air > 0.0f) {
        strcpy(g_SysState.status_mesin, "DOSING_AIR_BAKU");
        Actuator_SetInletValve(1, 0, 0);
        bool ok = Sensors_DoseFluid(FLUID_WATER, liter_air);
        Actuator_SetInletValve(0, 0, 0);
        Sensors_UpdateTankKalman(FLUID_WATER);
        if (!ok) goto fault_abort;
    }

    /* 2. Takar Nutrisi A (RLY2) melewati FLOW1 */
    if (liter_nut_a > 0.0f) {
        strcpy(g_SysState.status_mesin, "DOSING_NUTRISI_A");
        Actuator_SetInletValve(0, 1, 0);
        bool ok = Sensors_DoseFluid(FLUID_NUTRIENT_A, liter_nut_a);
        Actuator_SetInletValve(0, 0, 0);
        Sensors_UpdateTankKalman(FLUID_NUTRIENT_A);
        if (!ok) goto fault_abort;
    }

    /* 3. Takar Nutrisi B (RLY3) melewati FLOW1 */
    if (liter_nut_b > 0.0f) {
        strcpy(g_SysState.status_mesin, "DOSING_NUTRISI_B");
        Actuator_SetInletValve(0, 0, 1);
        bool ok = Sensors_DoseFluid(FLUID_NUTRIENT_B, liter_nut_b);
        Actuator_SetInletValve(0, 0, 0);
        Sensors_UpdateTankKalman(FLUID_NUTRIENT_B);
        if (!ok) goto fault_abort;
    }

    /* 4. Sirkulasi Homogenisasi Tanpa Aduk Manual (RLY5 + Pompa RLY6) */
    if (liter_nut_a > 0.0f || liter_nut_b > 0.0f) {
        strcpy(g_SysState.status_mesin, "MIXING_CIRCULATION");
        printf("[MIXING] Sirkulasi homogenisasi selama %lu ms...\n", MIXING_CIRC_TIME_MS);
        Actuator_StartMixingCirculation();
        osDelay(MIXING_CIRC_TIME_MS);
        Actuator_StopPumpSafe();
    }

    /* 5. Distribusi Irigasi ke Lahan (RLY4 + Pompa RLY6 melewati FLOW2) */
    if (liter_out_irigasi > 0.0f) {
        if (Sensors_IsTankEmpty()) {
            printf("[SAFETY] Tandon Mixing kosong! Batalkan penyiraman.\n");
            goto fault_abort;
        }
        strcpy(g_SysState.status_mesin, "DISTRIBUSI_LAHAN");
        Actuator_StartIrrigationPump();
        bool ok = Sensors_DoseFluid(FLUID_IRRIGATION_MIX, liter_out_irigasi);
        Actuator_StopPumpSafe();
        Sensors_UpdateTankKalman(FLUID_IRRIGATION_MIX);
        if (!ok) goto fault_abort;
    }

    Actuator_SetRGB(0, 1, 0); // Hijau: Sukses & Kembali Normal
    return true;

fault_abort:
    Actuator_StopAll();
    Actuator_SetRGB(1, 0, 0); // Merah: Terjadi Error / Aliran Mampet
    strcpy(g_SysState.status_mesin, "FAULT_FLOW_OR_EMPTY");
    return false;
}
