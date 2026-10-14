/*
 * drv_sensors.c
 *
 *  Created on: Oct 14, 2026
 *      Author: lenov
 */

#include "drv_sensors.h"
#include "cmsis_os.h"
#include <stdio.h>

volatile uint32_t g_flow1_pulses = 0;
volatile uint32_t g_flow2_pulses = 0;

TankKalman_t g_TankKalman = {
    .volume_est_liters = 5.0f,
    .distance_raw_cm   = 15.0f,
    .p_cov             = 1.0f,
    .q_process_noise   = 0.005f,
    .r_meas_noise      = 0.250f,
    .last_flow1_cnt    = 0,
    .last_flow2_cnt    = 0
};

static void DWT_Delay_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us_dwt(uint32_t us) {
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (HAL_RCC_GetHCLKFreq() / 1000000UL);
    while ((DWT->CYCCNT - start) < ticks);
}

static float GetFluidCalibrationFactor(FluidType_t fluid) {
    switch (fluid) {
        case FLUID_WATER:          return K_PULSE_BASE_FLOW1 * RHO_AIR;
        case FLUID_NUTRIENT_A:     return K_PULSE_BASE_FLOW1 * RHO_NUTRISI_A;
        case FLUID_NUTRIENT_B:     return K_PULSE_BASE_FLOW1 * RHO_NUTRISI_B;
        case FLUID_IRRIGATION_MIX: return K_PULSE_BASE_FLOW2;
        default:                   return K_PULSE_BASE_FLOW1;
    }
}

void Sensors_Init(void) {
    DWT_Delay_Init();
    float d = Sensors_ReadUltrasonicCM();
    if (d > 0.0f && d <= TANK_HEIGHT_CM) {
        g_TankKalman.distance_raw_cm   = d;
        g_TankKalman.volume_est_liters = ((TANK_HEIGHT_CM - d) * TANK_AREA_CM2) / 1000.0f;
    }
}

void Sensors_Flow1_ISR(void) {
    g_flow1_pulses++;
}

void Sensors_Flow2_ISR(void) {
    g_flow2_pulses++;
}

float Sensors_ReadUltrasonicCM(void) {
    HAL_GPIO_WritePin(US1_TRIG_GPIO_Port, US1_TRIG_Pin, GPIO_PIN_RESET);
    delay_us_dwt(2);
    HAL_GPIO_WritePin(US1_TRIG_GPIO_Port, US1_TRIG_Pin, GPIO_PIN_SET);
    delay_us_dwt(10);
    HAL_GPIO_WritePin(US1_TRIG_GPIO_Port, US1_TRIG_Pin, GPIO_PIN_RESET);

    uint32_t timeout = 200000;
    while (HAL_GPIO_ReadPin(US1_ECHO_GPIO_Port, US1_ECHO_Pin) == GPIO_PIN_RESET) {
        if (--timeout == 0) return -1.0f;
    }

    uint32_t start_cycle = DWT->CYCCNT;
    timeout = 500000;
    while (HAL_GPIO_ReadPin(US1_ECHO_GPIO_Port, US1_ECHO_Pin) == GPIO_PIN_SET) {
        if (--timeout == 0) return -1.0f;
    }

    uint32_t elapsed_cycles = DWT->CYCCNT - start_cycle;
    float duration_us = (float)elapsed_cycles / ((float)HAL_RCC_GetHCLKFreq() / 1000000.0f);
    return (duration_us * 0.0343f) / 2.0f;
}

float Sensors_UpdateTankKalman(FluidType_t active_inlet_fluid) {
    /* 1. TAHAP PREDIKSI: Integrasi pulsa FLOW1 (Masuk) & FLOW2 (Keluar) */
    uint32_t curr_f1 = g_flow1_pulses;
    uint32_t curr_f2 = g_flow2_pulses;

    uint32_t df1 = curr_f1 - g_TankKalman.last_flow1_cnt;
    uint32_t df2 = curr_f2 - g_TankKalman.last_flow2_cnt;
    g_TankKalman.last_flow1_cnt = curr_f1;
    g_TankKalman.last_flow2_cnt = curr_f2;

    float k_in   = GetFluidCalibrationFactor(active_inlet_fluid);
    float dv_in  = (float)df1 / k_in;
    float dv_out = (float)df2 / K_PULSE_BASE_FLOW2;

    g_TankKalman.volume_est_liters += (dv_in - dv_out);
    if (g_TankKalman.volume_est_liters < 0.0f) {
        g_TankKalman.volume_est_liters = 0.0f;
    }
    g_TankKalman.p_cov += g_TankKalman.q_process_noise;

    /* 2. TAHAP UPDATE: Koreksi dengan jarak Ultrasonik US1 */
    float dist_cm = Sensors_ReadUltrasonicCM();
    if (dist_cm > 0.0f && dist_cm <= (TANK_HEIGHT_CM + 5.0f)) {
        g_TankKalman.distance_raw_cm = dist_cm;
        float level_cm = TANK_HEIGHT_CM - dist_cm;
        if (level_cm < 0.0f) level_cm = 0.0f;

        float vol_meas_liters = (level_cm * TANK_AREA_CM2) / 1000.0f;

        float kalman_gain = g_TankKalman.p_cov / (g_TankKalman.p_cov + g_TankKalman.r_meas_noise);
        g_TankKalman.volume_est_liters += kalman_gain * (vol_meas_liters - g_TankKalman.volume_est_liters);
        g_TankKalman.p_cov = (1.0f - kalman_gain) * g_TankKalman.p_cov;
    }

    return g_TankKalman.volume_est_liters;
}

bool Sensors_IsTankEmpty(void) {
    float min_safe_vol = ((TANK_HEIGHT_CM - TANK_EMPTY_DIST_CM) * TANK_AREA_CM2) / 1000.0f;
    return (g_TankKalman.volume_est_liters <= min_safe_vol);
}

bool Sensors_DoseFluid(FluidType_t fluid, float target_liters) {
    if (target_liters <= 0.0f) return true;

    volatile uint32_t *p_counter = (fluid == FLUID_IRRIGATION_MIX) ? &g_flow2_pulses : &g_flow1_pulses;
    float k_factor = GetFluidCalibrationFactor(fluid);
    uint32_t target_pulses = (uint32_t)(target_liters * k_factor);

    uint32_t start_pulses   = *p_counter;
    uint32_t last_pulses    = start_pulses;
    uint32_t stall_timer_ms = 0;

    printf("[DOSING] Target %.2f L (%lu pulsa) pada fluida #%d\n",
           target_liters, target_pulses, (int)fluid);

    while ((*p_counter - start_pulses) < target_pulses) {
        osDelay(100);
        stall_timer_ms += 100;

        uint32_t current_pulses = *p_counter;
        if (current_pulses > last_pulses) {
            last_pulses    = current_pulses;
            stall_timer_ms = 0; // Reset watchdog selama cairan mengalir
        } else if (stall_timer_ms >= FLOW_STALL_TIMEOUT_MS) {
            printf("[ERROR] Aliran macet (Flow Timeout) pada fluida #%d!\n", (int)fluid);
            return false;
        }
    }

    printf("[DOSING] Selesai menakar fluida #%d.\n", (int)fluid);
    return true;
}
