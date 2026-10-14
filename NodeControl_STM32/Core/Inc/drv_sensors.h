/*
 * drv_sensors.h
 *
 *  Created on: Oct 14, 2026
 *      Author: lenov
 */

#ifndef INC_DRV_SENSORS_H_
#define INC_DRV_SENSORS_H_

#ifndef DRV_SENSORS_H
#define DRV_SENSORS_H

#include "soga_config.h"

typedef enum {
    FLUID_WATER = 0,
    FLUID_NUTRIENT_A,
    FLUID_NUTRIENT_B,
    FLUID_IRRIGATION_MIX
} FluidType_t;

typedef struct {
    float volume_est_liters; // Estimasi volume tandon hasil Kalman (Liter)
    float distance_raw_cm;   // Jarak mentah ultrasonik US1 (cm)
    float p_cov;             // Error covariance Kalman
    float q_process_noise;   // Variansi model integrasi Flow Sensor
    float r_meas_noise;      // Variansi pengukuran Ultrasonik US1
    uint32_t last_flow1_cnt;
    uint32_t last_flow2_cnt;
} TankKalman_t;

extern volatile uint32_t g_flow1_pulses;
extern volatile uint32_t g_flow2_pulses;
extern TankKalman_t g_TankKalman;

void Sensors_Init(void);
void Sensors_Flow1_ISR(void);
void Sensors_Flow2_ISR(void);

float Sensors_ReadUltrasonicCM(void);
float Sensors_UpdateTankKalman(FluidType_t active_inlet_fluid);
bool  Sensors_IsTankEmpty(void);
bool  Sensors_DoseFluid(FluidType_t fluid, float target_liters);

#endif /* DRV_SENSORS_H */

#endif /* INC_DRV_SENSORS_H_ */
