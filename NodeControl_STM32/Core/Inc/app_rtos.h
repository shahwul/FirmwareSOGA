/*
 * app_rtos.h
 *
 *  Created on: Oct 14, 2026
 *      Author: lenov
 */

#ifndef INC_APP_RTOS_H_
#define INC_APP_RTOS_H_

#ifndef APP_RTOS_H
#define APP_RTOS_H

#include "soga_config.h"

void App_System_Init(void);
void App_TaskTelemetry_Run(void);
void App_TaskIrrigation_Run(void);

#endif /* APP_RTOS_H */

#endif /* INC_APP_RTOS_H_ */
