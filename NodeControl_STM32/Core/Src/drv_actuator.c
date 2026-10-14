/*
 * drv_actuator.c
 *
 *  Created on: Oct 14, 2026
 *      Author: lenov
 */

#include "drv_actuator.h"
#include "cmsis_os.h"

SogaActuator_t g_Actuators = {0, 0, 0, 0, 0, 0};

void Actuator_Init(void) {
    Actuator_StopAll();
    Actuator_SetRGB(0, 1, 0); // Hijau: Standby
}

void Actuator_ApplyState(const SogaActuator_t *state) {
    /*
     * SAFETY INTERLOCK:
     * Pompa (RLY6) tidak boleh menyala jika jalur keluar (RLY4 Irigasi & RLY5 Mixing)
     * keduanya tertutup.
     */
    uint8_t safe_pump = state->rly6_pump;
    if (safe_pump && (state->rly4_irigasi == 0) && (state->rly5_mix_loop == 0)) {
        safe_pump = 0;
    }

    HAL_GPIO_WritePin(RLY1_GPIO_Port, RLY1_Pin, state->rly1_air      ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RLY2_GPIO_Port, RLY2_Pin, state->rly2_nut_a    ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RLY3_GPIO_Port, RLY3_Pin, state->rly3_nut_b    ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RLY4_GPIO_Port, RLY4_Pin, state->rly4_irigasi  ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RLY5_GPIO_Port, RLY5_Pin, state->rly5_mix_loop ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RLY6_GPIO_Port, RLY6_Pin, safe_pump            ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Actuator_StopAll(void) {
    g_Actuators = (SogaActuator_t){0, 0, 0, 0, 0, 0};
    Actuator_ApplyState(&g_Actuators);
}

void Actuator_SetInletValve(uint8_t air, uint8_t nut_a, uint8_t nut_b) {
    g_Actuators.rly1_air   = air;
    g_Actuators.rly2_nut_a = nut_a;
    g_Actuators.rly3_nut_b = nut_b;
    Actuator_ApplyState(&g_Actuators);
}

void Actuator_StartMixingCirculation(void) {
    g_Actuators.rly4_irigasi  = 0;
    g_Actuators.rly5_mix_loop = 1; // Buka jalur balik ke tandon mixing
    Actuator_ApplyState(&g_Actuators);
    osDelay(VALVE_SETTLE_DELAY_MS);
    g_Actuators.rly6_pump     = 1; // Nyalakan pompa setelah valve terbuka
    Actuator_ApplyState(&g_Actuators);
}

void Actuator_StartIrrigationPump(void) {
    g_Actuators.rly5_mix_loop = 0;
    g_Actuators.rly4_irigasi  = 1; // Buka jalur distribusi irigasi
    Actuator_ApplyState(&g_Actuators);
    osDelay(VALVE_SETTLE_DELAY_MS);
    g_Actuators.rly6_pump     = 1; // Nyalakan pompa setelah valve terbuka
    Actuator_ApplyState(&g_Actuators);
}

void Actuator_StopPumpSafe(void) {
    g_Actuators.rly6_pump = 0;     // Matikan pompa dulu
    Actuator_ApplyState(&g_Actuators);
    osDelay(VALVE_SETTLE_DELAY_MS);
    g_Actuators.rly4_irigasi  = 0; // Baru tutup semua valve keluaran
    g_Actuators.rly5_mix_loop = 0;
    Actuator_ApplyState(&g_Actuators);
}

void Actuator_SetRGB(uint8_t r, uint8_t g, uint8_t b) {
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, b ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
