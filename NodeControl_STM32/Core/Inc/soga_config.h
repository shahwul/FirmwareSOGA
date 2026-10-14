/*
 * soga_config.h
 *
 *  Created on: Oct 14, 2026
 *      Author: lenov
 */

#ifndef INC_SOGA_CONFIG_H_
#define INC_SOGA_CONFIG_H_

#ifndef SOGA_CONFIG_H
#define SOGA_CONFIG_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/* ================= 1. KALIBRASI FLOW & MASSA JENIS CAIRAN =========== */
/*
 * FLOW1 (PB9) berada di percabangan 3 tandon menuju Tandon Mixing.
 * Konstanta pulsa/liter efektif disesuaikan dengan viskositas & massa jenis:
 * Pulsa_Target = Target_Liter * K_PULSE_BASE_FLOW1 * (RHO_FLUIDA / RHO_AIR)
 */
#define K_PULSE_BASE_FLOW1      450.0f   // Pulsa/Liter dasar sensor FLOW1 (Inlet)
#define K_PULSE_BASE_FLOW2      450.0f   // Pulsa/Liter dasar sensor FLOW2 (Irigasi)

#define RHO_AIR                 1.000f   // Massa jenis Air Baku (kg/L)
#define RHO_NUTRISI_A           1.035f   // Massa jenis Pekatan Nutrisi A (kg/L)
#define RHO_NUTRISI_B           1.040f   // Massa jenis Pekatan Nutrisi B (kg/L)

/* ================= 2. GEOMETRI TANDON MIXING (KALMAN FILTER) ======== */
#define TANK_HEIGHT_CM          30.0f    // Jarak sensor US1 ke dasar tandon (cm)
#define TANK_EMPTY_DIST_CM      27.0f    // Jarak >= 27cm berarti tinggi air <= 3cm (Kosong)
#define TANK_AREA_CM2           706.86f  // Luas alas tandon (cm^2) - contoh D = 30 cm

/* ================= 3. PARAMETER KONTROL VPD & IRIGASI =============== */
#define DWELL_TIME_MS           900000UL // 15 menit waktu tunggu resapan tanah
#define MIXING_CIRC_TIME_MS     10000UL  // 10 detik sirkulasi pengadukan (RLY5 + RLY6)
#define VALVE_SETTLE_DELAY_MS   250UL    // Jeda buka/tutup solenoid valve (anti water-hammer)
#define FLOW_STALL_TIMEOUT_MS   5000UL   // 5 detik proteksi mampet / tandon sumber habis

#define DEADBAND_PCT            5.0f     // Histeresis kelembapan tanah (%)
#define SP_BASE_PCT             70.0f    // Setpoint dasar kelembapan tanah (%)
#define SP_VPD_GAIN             5.0f     // Kenaikan SP (%) per 1.0 kPa VPD
#define SP_MIN_PCT              70.0f    // Batas bawah Setpoint VMC (%)
#define SP_MAX_PCT              80.0f    // Batas atas Setpoint VMC (%)

/* ================= 4. RESEP DEFAULT PER SIKLUS MIXING =============== */
#define DEFAULT_RECIPE_AIR_L    4.00f    // 4.0 Liter Air Baku (RLY1)
#define DEFAULT_RECIPE_NUT_A_L  0.10f    // 0.1 Liter (100 mL) Nutrisi A (RLY2)
#define DEFAULT_RECIPE_NUT_B_L  0.10f    // 0.1 Liter (100 mL) Nutrisi B (RLY3)
#define DEFAULT_RECIPE_OUT_L    1.50f    // 1.5 Liter disiram ke lahan per siklus (RLY4)

#endif /* SOGA_CONFIG_H */

#endif /* INC_SOGA_CONFIG_H_ */
