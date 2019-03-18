/*
 * project_settings.h
 *
 *  Created on: Mar 1, 2019
 *      Author: sglas
 */

#ifndef PROJECT_SETTINGS_H_
#define PROJECT_SETTINGS_H_

#define FCPU 24000000 //1048576

// include the library header
#include "library.h"
// let the system know which lower level modules are in use
// this way higher modules can selectively utilize their resources
#define USE_MODULE_TASK
#define USE_MODULE_TIMING
#define USE_MODULE_LIST
#define USE_MODULE_BUFFER
#define USE_MODULE_BUFFER_PRINTF
#define USE_MODULE_UART
#define USE_MODULE_SUBSYSTEM


/* Highly recommended to use backchannel UART (0) instead of
 * Application UART (1) because backchannel is much slower
 * and does make for a good gaming experience.
 *
 * 3.4 green on cable
 * 3.3 white on cable
 */
#define UART0_TX_BUFFER_LENGTH 512
#define USE_UART0
#define SUBSYSTEM_IO SUBSYSTEM_IO_UART
#define SUBSYSTEM_UART 0

#define TASK_MAX_LENGTH 50


#endif /* PROJECT_SETTINGS_H_ */
