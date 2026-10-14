/*
 * app_rtos.c
 *
 *  Created on: Oct 14, 2026
 *      Author: lenov
 */

#include "app_rtos.h"
#include "drv_actuator.h"
#include "drv_sensors.h"
#include "ctrl_fertigation.h"
#include "cmsis_os.h"
#include "usbd_cdc_if.h"
#include <stdio.h>
#include <string.h>

/* Handle dari main.c hasil generate .ioc */
extern UART_HandleTypeDef huart2;
extern RTC_HandleTypeDef  hrtc;
extern osThreadId         TaskTelemetryHandle;
extern osThreadId         TaskIrrigationHandle;

static uint8_t  s_rxByte = 0;
static char     s_rxBuffer[512];
static volatile uint16_t s_rxIndex = 0;

/* Redirect printf ke USB Type-C Virtual COM Port */
int _write(int file, char *ptr, int len) {
    (void)file;
    CDC_Transmit_FS((uint8_t *)ptr, len);
    osDelay(2);
    return len;
}

static void Report_ControlState(float vpd, float sp, float pv, float error, const char *action) {
    char buf[220];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"control_state\",\"vpd\":%.2f,\"sp\":%.1f,\"pv\":%.1f,"
             "\"error\":%.1f,\"action\":\"%s\"}\n",
             vpd, sp, pv, error, action);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 100);
}

/* Helper untuk mem-parsing array 5 sensor VMC: "soil_moisture":[v1,v2,v3,v4,v5] */
static bool Parse_SoilMoistureArray(const char *json, float *out_avg) {
    const char *p = strstr(json, "\"soil_moisture\":[");
    if (!p) return false;

    float v[5] = {0};
    int count = sscanf(p, "\"soil_moisture\":[%f,%f,%f,%f,%f",
                       &v[0], &v[1], &v[2], &v[3], &v[4]);
    if (count <= 0) return false;

    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        sum += v[i];
    }
    *out_avg = sum / (float)count;
    return true;
}

static void Process_InboundJSON(const char *json) {
    /* 1. Sinkronisasi RTC dari ESP32-C3 */
    if (strstr(json, "\"cmd\":\"sync_rtc\"") != NULL) {
        int jam = 0, menit = 0, detik = 0;
        const char *ph = strstr(json, "\"h\":");
        const char *pm = strstr(json, "\"m\":");
        const char *ps = strstr(json, "\"s\":");
        if (ph) sscanf(ph, "\"h\":%d", &jam);
        if (pm) sscanf(pm, "\"m\":%d", &menit);
        if (ps) sscanf(ps, "\"s\":%d", &detik);

        RTC_TimeTypeDef sTime = {0};
        sTime.Hours      = (uint8_t)jam;
        sTime.Minutes    = (uint8_t)menit;
        sTime.Seconds    = (uint8_t)detik;
        sTime.TimeFormat = RTC_HOURFORMAT_24;
        HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        printf("[RTC] Sync sukses -> %02d:%02d:%02d\n", jam, menit, detik);
    }

    /* 2. Baca Data VPD (Node DHT) & Array 5 Sensor VMC (Node VMC) */
    const char *ptr_vpd = strstr(json, "\"vpd\":");
    if (ptr_vpd) {
        sscanf(ptr_vpd, "\"vpd\":%f", &g_SysState.current_vpd);
    }

    bool got_soil = Parse_SoilMoistureArray(json, &g_SysState.current_moisture);

    if (ptr_vpd != NULL || got_soil) {
        g_SysState.dynamic_sp = Fertigation_ComputeDynamicSP(g_SysState.current_vpd);
        float error = g_SysState.dynamic_sp - g_SysState.current_moisture;
        const char *action = "idle";

        if (g_SysState.latch_dosing_event) {
            action = "watering";
            g_SysState.latch_dosing_event = 0;
        } else if (g_SysState.latch_nutrient_event) {
            action = "nutrient_dosing";
            g_SysState.latch_nutrient_event = 0;
        } else if (strcmp(g_SysState.status_mesin, "WAIT_ABSORPTION") == 0) {
            action = "within_deadband";
        } else if (error > DEADBAND_PCT) {
            action = "ready_to_water";
        } else {
            action = "within_deadband";
        }

        Report_ControlState(g_SysState.current_vpd, g_SysState.dynamic_sp,
                            g_SysState.current_moisture, error, action);
    }

    /* 3. Update Resep Dinamis (Jika dikirim dari Dashboard) */
    const char *p_air = strstr(json, "\"vol_air\":");
    const char *p_na  = strstr(json, "\"vol_a\":");
    const char *p_nb  = strstr(json, "\"vol_b\":");
    const char *p_out = strstr(json, "\"vol_out\":");
    if (p_air) sscanf(p_air, "\"vol_air\":%f", &g_SysState.recipe_air_L);
    if (p_na)  sscanf(p_na,  "\"vol_a\":%f",   &g_SysState.recipe_nut_a_L);
    if (p_nb)  sscanf(p_nb,  "\"vol_b\":%f",   &g_SysState.recipe_nut_b_L);
    if (p_out) sscanf(p_out, "\"vol_out\":%f", &g_SysState.recipe_out_L);

    /* 4. Perintah Eksekusi Manual & Pergantian Mode */
    if (strstr(json, "\"siram\":1") != NULL)         g_SysState.flag_manual_air = 1;
    if (strstr(json, "\"siram_nutrisi\":1") != NULL) g_SysState.flag_manual_nutrisi = 1;

    if (strstr(json, "\"auto_mode\":1") != NULL) {
        g_SysState.is_auto_mode = 1;
        printf("[MODE] Beralih ke OTOMATIS\n");
    } else if (strstr(json, "\"auto_mode\":0") != NULL) {
        g_SysState.is_auto_mode = 0;
        printf("[MODE] Beralih ke MANUAL\n");
    }
}

void App_System_Init(void) {
    Actuator_Init();
    Sensors_Init();
    Fertigation_Init();
}

void App_TaskTelemetry_Run(void) {
    char jsonTxBuffer[320];
    uint32_t last_tx_time = osKernelSysTick();

    HAL_UART_Receive_IT(&huart2, &s_rxByte, 1);
    printf("[SYSTEM] TaskTelemetry Siap di USART2!\n");

    for (;;) {
        uint32_t rx_notif = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
        if (rx_notif > 0) {
            Process_InboundJSON(s_rxBuffer);
            memset(s_rxBuffer, 0, sizeof(s_rxBuffer));
            s_rxIndex = 0;
        }

        if ((osKernelSysTick() - last_tx_time) >= 5000) {
            float vol_kalman = Sensors_UpdateTankKalman(FLUID_WATER);
            uint32_t uptime  = osKernelSysTick() / 1000;
            uint32_t heap    = xPortGetFreeHeapSize();

            snprintf(jsonTxBuffer, sizeof(jsonTxBuffer),
                     "{\"type\":\"heartbeat\",\"node\":\"SOGA-CTRL-01\",\"status\":\"%s\","
                     "\"tank_vol_L\":%.2f,\"tank_dist_cm\":%.1f,\"is_tank_empty\":%d,"
                     "\"vpd\":%.2f,\"sp\":%.1f,\"vmc\":%.1f,\"is_auto_mode\":%d,"
                     "\"uptime\":%lu,\"free_heap\":%lu}\n",
                     g_SysState.status_mesin, vol_kalman, g_TankKalman.distance_raw_cm,
                     Sensors_IsTankEmpty() ? 1 : 0, g_SysState.current_vpd,
                     g_SysState.dynamic_sp, g_SysState.current_moisture,
                     g_SysState.is_auto_mode, uptime, heap);

            HAL_UART_Transmit(&huart2, (uint8_t *)jsonTxBuffer, strlen(jsonTxBuffer), 100);
            last_tx_time = osKernelSysTick();
        }

        osDelay(10);
    }
}

void App_TaskIrrigation_Run(void) {
    static uint8_t  last_minute_triggered = 60;
    static uint8_t  is_waiting_absorption = 0;
    static uint32_t time_start_absorption = 0;

    Actuator_StopAll();

    for (;;) {
        RTC_TimeTypeDef sTime;
        RTC_DateTypeDef sDate;
        HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

        g_SysState.dynamic_sp = Fertigation_ComputeDynamicSP(g_SysState.current_vpd);

        /* ========================================================
         * RULE 1: TRIGGER NUTRISI AB MIX + SIRKULASI + DISTRIBUSI
         * ======================================================== */
        if (((g_SysState.is_auto_mode && (sTime.Minutes % 30 == 0)) &&
             (sTime.Minutes != last_minute_triggered)) || g_SysState.flag_manual_nutrisi) {

            last_minute_triggered = sTime.Minutes;
            g_SysState.latch_nutrient_event = 1;

            /* Jika tandon mixing kosong, racik Air + Nutrisi A + Nutrisi B terlebih dahulu */
            bool need_refill = Sensors_IsTankEmpty();
            float fill_air   = need_refill ? g_SysState.recipe_air_L   : 0.0f;
            float fill_a     = need_refill ? g_SysState.recipe_nut_a_L : 0.0f;
            float fill_b     = need_refill ? g_SysState.recipe_nut_b_L : 0.0f;

            bool ok = Fertigation_RunCycle(fill_air, fill_a, fill_b, g_SysState.recipe_out_L);

            if (g_SysState.flag_manual_nutrisi) {
                const char *ack = ok
                    ? "{\"type\":\"ack\",\"node\":\"SOGA-CTRL-01\",\"status\":\"DONE\",\"keterangan\":\"Siram Nutrisi AB Sukses\"}\n"
                    : "{\"type\":\"ack\",\"node\":\"SOGA-CTRL-01\",\"status\":\"ERROR\",\"keterangan\":\"Aliran Mampet atau Tandon Kosong\"}\n";
                HAL_UART_Transmit(&huart2, (uint8_t *)ack, strlen(ack), 100);
                g_SysState.flag_manual_nutrisi = 0;
            }

            if (ok) {
                strcpy(g_SysState.status_mesin, is_waiting_absorption ? "WAIT_ABSORPTION" : "STANDBY");
            }
        }

        /* ========================================================
         * RULE 2: TRIGGER PENYIRAMAN BERBASIS VMC & VPD (DWELL TIME)
         * ======================================================== */
        if (is_waiting_absorption) {
            if ((osKernelSysTick() - time_start_absorption) >= DWELL_TIME_MS) {
                is_waiting_absorption = 0;
                strcpy(g_SysState.status_mesin, "STANDBY");
                printf("[IRIGASI] Masa resapan selesai, siap evaluasi sensor kembali.\n");
            }
        } else {
            float threshold = g_SysState.dynamic_sp - DEADBAND_PCT;
            if ((g_SysState.is_auto_mode && (g_SysState.current_moisture < threshold)) ||
                g_SysState.flag_manual_air) {

                g_SysState.latch_dosing_event = 1;
                float topup_air = Sensors_IsTankEmpty() ? g_SysState.recipe_air_L : 0.0f;

                bool ok = Fertigation_RunCycle(topup_air, 0.0f, 0.0f, g_SysState.recipe_out_L);

                if (g_SysState.flag_manual_air) {
                    const char *ack = ok
                        ? "{\"type\":\"ack\",\"node\":\"SOGA-CTRL-01\",\"status\":\"DONE\",\"keterangan\":\"Siram Air Sukses\"}\n"
                        : "{\"type\":\"ack\",\"node\":\"SOGA-CTRL-01\",\"status\":\"ERROR\",\"keterangan\":\"Aliran Mampet atau Tandon Kosong\"}\n";
                    HAL_UART_Transmit(&huart2, (uint8_t *)ack, strlen(ack), 100);
                    g_SysState.flag_manual_air = 0;
                }

                if (ok) {
                    is_waiting_absorption = 1;
                    time_start_absorption = osKernelSysTick();
                    strcpy(g_SysState.status_mesin, "WAIT_ABSORPTION");
                }
            }
        }

        osDelay(1000);
    }
}

/* ============================================================================
 * INTERRUPT CALLBACKS (USART2 & EXTI FLOW SENSOR PB9 / PB10)
 * ============================================================================ */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == FLOW1_Pin) {          // PB9: Flow 1 (Percabangan Inlet Mixing)
        Sensors_Flow1_ISR();
    } else if (GPIO_Pin == FLOW2_Pin) {   // PB10: Flow 2 (Jalur Distribusi Irigasi)
        Sensors_Flow2_ISR();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        if (s_rxByte == '\n') {
            s_rxBuffer[s_rxIndex] = '\0';
            s_rxIndex = 0;

            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            vTaskNotifyGiveFromISR(TaskTelemetryHandle, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        } else {
            if (s_rxIndex < sizeof(s_rxBuffer) - 1) {
                s_rxBuffer[s_rxIndex++] = s_rxByte;
            } else {
                s_rxIndex = 0;
            }
        }
        huart->Lock = HAL_UNLOCKED;
        HAL_UART_Receive_IT(&huart2, &s_rxByte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        huart->Lock = HAL_UNLOCKED;
        HAL_UART_Receive_IT(&huart2, &s_rxByte, 1);
    }
}
