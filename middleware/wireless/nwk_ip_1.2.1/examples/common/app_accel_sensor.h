/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*!
**  @addtogroup FX Accelerometer
**  
**  @{
*/
#ifndef _APP_ACCEL_SENSOR_H
#define _APP_ACCEL_SENSOR_H

/*!=================================================================================================
\file       app_accel_sensor.h
\brief      This is a header file for the accelerometer utility for socket app client demo.
==================================================================================================*/

/*==================================================================================================
Include Files
==================================================================================================*/

#include "EmbeddedTypes.h"

/*==================================================================================================
Public macros
==================================================================================================*/

#ifndef USE_ACCELEROMETER
  #define USE_ACCELEROMETER 1
#endif

#define ACCEL_BUFF_SIZE     (30U)

/*==================================================================================================
Public type definitions
==================================================================================================*/
/*!
 * @brief Callback pointer type definition for the accelerometer
 */
typedef void (*AccelFunction_t) ( uint8_t events );

/**
 * @brief Different strings that can be sent.
 *
 */
typedef enum accel_event_type_tag
{
    gAccel_X_c = 1,         /**< X */
    gAccel_Y_c,             /**< Y */
    gAccel_Z_c,             /**< Z */
    gAccel_All_c,           /**< All */
    gAccelMax_c,

}accel_event_type_t;

/*==================================================================================================
Public global variables declarations
==================================================================================================*/

/* None */

/*==================================================================================================
Public function prototypes
==================================================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================*/  

  
/* Accelerometer functions:*/
/*!*************************************************************************************************
\fn         status_t APP_InitAccelerometer(AccelFunction_t pfCallBackAdr)
\brief      Accelerometer initialization

\param [in] pfCallBackAdr Callback with event for application

\return     kStatus_Success if success or kStatus_Fail if error.

\note       When using this function make sure you don't exceed the number of gpio isr's defined 
            in gGpioMaxIsrEntries_c.
***************************************************************************************************/  
status_t APP_InitAccelerometer(AccelFunction_t pfCallBackAdr);

/*!*************************************************************************************************
\fn     status_t APP_EnableAccelerometer(void)
\brief  Enable accelerometer

\param  [in]    none

\return         kStatus_Success if success or kStatus_Fail if error.
    
***************************************************************************************************/ 
status_t APP_EnableAccelerometer(void);

/*!*************************************************************************************************
\fn     status_t APP_DisableAccelerometer(void)
\brief  Enable accelerometer

\param  [in]    none

\return         kStatus_Success if success or kStatus_Fail if error.
    
***************************************************************************************************/ 
status_t APP_DisableAccelerometer(void);

/*!*************************************************************************************************
\fn     void* App_GetAccelDataString(uint8_t accelEvent)
\brief  Return post data.

\param  [in]    Axis to read

\return         return data to be send through post
***************************************************************************************************/
void* App_GetAccelDataString(uint8_t accelEvent);

/*!*************************************************************************************************
\fn             void FX_Accel_Int_ISR(void)
\brief          Accelerometer interrupt handler

\return         void
***************************************************************************************************/
#if defined(__IAR_SYSTEMS_ICC__)
#pragma location = ".isr_handler"
#endif
void FX_Accel_Int_ISR
(
void
);

#ifdef __cplusplus
}
#endif
/*================================================================================================*/

#endif /* _ACCEL_SENSOR_H */
/*!
** @}
*/