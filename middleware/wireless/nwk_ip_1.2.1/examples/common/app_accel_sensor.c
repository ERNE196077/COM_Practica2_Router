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

/*!
\file       app_accel_sensor.c
\brief      This is a public source file for the application acclerometer. 
*/

/*==================================================================================================
Include Files
==================================================================================================*/

/* General Includes */
#include "EmbeddedTypes.h"
#include "network_utils.h"
#include "stdlib.h"

/* Drivers */
#include "board.h"
#include "fsl_fxos.h"
#include "fsl_i2c.h"
#include "gpio_pins.h"

#include "app_accel_sensor.h"
#include "FunctionLib.h"
#include "MemManager.h"
#include "TimersManager.h"
#include "GPIO_Adapter.h"

/*==================================================================================================
Private macros
==================================================================================================*/
#define gAccel_IsrPrio_c (0x80)

#define TIMER_200ms     200

#define EN_LOWPOWER         0

#define INTERRUPT_PIN_1     1
/*==================================================================================================
Private global variables declarations
==================================================================================================*/
#if USE_ACCELEROMETER

/*==================================================================================================
Public global variables declarations
==================================================================================================*/
tmrTimerID_t mAccelTimerID = gTmrInvalidTimerID_c; //wait Timer

static AccelFunction_t mpfAccelFunction = NULL;

static i2c_master_handle_t g_MasterHandle;
/* FXOS device address */
static const uint8_t g_accel_address[] = {0x1CU, 0x1DU, 0x1EU, 0x1FU};
static fxos_handle_t fxosHandle = {0};
/*==================================================================================================
Private prototypes
==================================================================================================*/
static void Accel_Interrupt_Enable(void);
/*==================================================================================================
Public functions
==================================================================================================*/
status_t APP_InitAccelerometer(AccelFunction_t pfCallBackAdr)
{
    i2c_master_config_t i2cConfig = {0};
    uint32_t i2cSourceClock = 0;
    uint8_t i = 0;
    uint8_t regResult = 0;
    uint8_t array_addr_size = 0;
    bool foundDevice = false;

    /* If no valid pointer provided, return */
    if(NULL == pfCallBackAdr)
    {
        return kStatus_Fail;
    }
    
    /* Store the pointer to callback function provided by the application */
    mpfAccelFunction = pfCallBackAdr;
    
    if(mAccelTimerID == gTmrInvalidTimerID_c) 
    {
        mAccelTimerID = TMR_AllocateTimer();
    }
    
    i2cSourceClock = CLOCK_GetFreq(I2C0_CLK_SRC);
    fxosHandle.base = I2C0;
    fxosHandle.i2cHandle = &g_MasterHandle;
        
    /* Initialize Acceleration Interrupt pin */
    //(void)GpioInputPinInit(&intAccelPin, 1);

    // Make sure you don't exceed the number of gpio isr's defined in gGpioMaxIsrEntries_c
    GpioInstallIsr(FX_Accel_Int_ISR, gGpioIsrPrioLow_c, gAccel_IsrPrio_c, &intAccelPin);
    
    I2C_MasterGetDefaultConfig(&i2cConfig);
    I2C_MasterInit(I2C0, &i2cConfig, i2cSourceClock);
    I2C_MasterTransferCreateHandle(I2C0, &g_MasterHandle, NULL, NULL);

    /* Find sensor devices */
    array_addr_size = sizeof(g_accel_address) / sizeof(g_accel_address[0]);
    for (i = 0; i < array_addr_size; i++)
    {
        fxosHandle.xfer.slaveAddress = g_accel_address[i];
        if (FXOS_ReadReg(&fxosHandle, WHO_AM_I_REG, &regResult, 1) == kStatus_Success)
        {
            foundDevice = true;
            break;
        }
        if ((i == (array_addr_size - 1)) && (!foundDevice))
        {
            return kStatus_Fail;
        }
    }

    uint8_t tmp[1] = {0};

    if(FXOS_ReadReg(&fxosHandle, WHO_AM_I_REG, tmp, 1) != kStatus_Success){return kStatus_Fail;}
    if (tmp[0] != kFXOS_WHO_AM_I_Device_ID){return kStatus_Fail;}

    /* go to standby */
    if(FXOS_ReadReg(&fxosHandle, CTRL_REG1, tmp, 1) != kStatus_Success){return kStatus_Fail;}
    if(FXOS_WriteReg(&fxosHandle, CTRL_REG1, tmp[0] & (uint8_t)~ACTIVE_MASK) != kStatus_Success){return kStatus_Fail;}

    /* Read again to make sure we are in standby mode. */
    if(FXOS_ReadReg(&fxosHandle, CTRL_REG1, tmp, 1) != kStatus_Success){return kStatus_Fail;}
    if ((tmp[0] & ACTIVE_MASK) == ACTIVE_MASK){return kStatus_Fail;}

    /* Event latching enabled, x,y,z enable, HPF NOT bypassed */
    if(FXOS_WriteReg(&fxosHandle, TRANSIENT_CFG_REG, TELE_MASK | ZTEFE_MASK | YTEFE_MASK | XTEFE_MASK) != kStatus_Success){return kStatus_Fail;}
    /* Debounce behavior = clear when condition not true, Threshold 63mg x 1 = 63mg */
    if(FXOS_WriteReg(&fxosHandle, TRANSIENT_THS_REG, DBCNTM_MASK | THS0_MASK) != kStatus_Success){return kStatus_Fail;}
    /* Transient count = 80ms (12.5Hz) x 2 = 160ms */
    if(FXOS_WriteReg(&fxosHandle, TRANSIENT_COUNT_REG, 0x02) != kStatus_Success){return kStatus_Fail;}

    /* Active low interrupts, Open-drain */
    if(FXOS_WriteReg(&fxosHandle, CTRL_REG3, PP_OD_MASK) != kStatus_Success){return kStatus_Fail;}
    /* enable data-ready, auto-sleep and motion detection interrupts */
    if(FXOS_WriteReg(&fxosHandle, CTRL_REG4, INT_EN_TRANS_MASK) != kStatus_Success){return kStatus_Fail;}

#if INTERRUPT_PIN_1
    /* route tansient interrupt to INT1*/
    if(FXOS_WriteReg(&fxosHandle, CTRL_REG5, INT_CFG_TRANS_MASK) != kStatus_Success){return kStatus_Fail;}
#else
    /* route tansient interrupt to INT2*/
    if(FXOS_WriteReg(&fxosHandle, CTRL_REG5, 0x0) != kStatus_Success){return kStatus_Fail;}
#endif

#if EN_LOWPOWER
    /* Disable auto-sleep, low power in wake */
    if(FXOS_WriteReg(&fxosHandle, CTRL_REG2, MOD_LOW_POWER) != kStatus_Success){return kStatus_Fail;}
    /* finally activate accel_device with ODR = 12.5Hz, FSR=2g */
    if(FXOS_WriteReg(&fxosHandle, CTRL_REG1, DATA_RATE_12_5HZ) != kStatus_Success){return kStatus_Fail;}
#else
    /* Disable auto-sleep, Normal power mode in wake */
    if(FXOS_WriteReg(&fxosHandle, CTRL_REG2, MOD_NORMAL) != kStatus_Success){return kStatus_Fail;}
    /* finally activate accel_device with ODR = 12.5Hz, FSR=2g */
    if(FXOS_WriteReg(&fxosHandle, CTRL_REG1, DATA_RATE_800HZ) != kStatus_Success){return kStatus_Fail;}
#endif
        
    return kStatus_Success;
}

status_t APP_EnableAccelerometer(void)
{
    uint8_t tmp[1] = {0};

    /* Read Control register 1 */
    if(FXOS_ReadReg(&fxosHandle, CTRL_REG1, tmp, 1) != kStatus_Success){return kStatus_Fail;}
    
    /* Enable accelerometer */
    if(FXOS_WriteReg(&fxosHandle, CTRL_REG1, tmp[0] | ACTIVE_MASK) != kStatus_Success){return kStatus_Fail;}
    
    return kStatus_Success;
}

status_t APP_DisableAccelerometer(void)
{
    uint8_t tmp[1] = {0};

    /* Read Control register 1 */
    if(FXOS_ReadReg(&fxosHandle, CTRL_REG1, tmp, 1) != kStatus_Success){return kStatus_Fail;}
    
    /* Disable accelerometer */
    if(FXOS_WriteReg(&fxosHandle, CTRL_REG1, tmp[0] & ~ACTIVE_MASK) != kStatus_Success){return kStatus_Fail;}
    
    return kStatus_Success;
}

#if defined(__IAR_SYSTEMS_ICC__)
#pragma location = ".isr_handler"
#endif
void FX_Accel_Int_ISR
(
void
)
{
    uint8_t tmp[1] = {0};
        
    if(GpioIsPinIntPending(&intAccelPin))
    {
        GpioClearPinIntFlag(&intAccelPin);
    }

    /* Disable interrupt */
    intAccelPin.interruptSelect = pinInt_Disabled_c;
    (void)GpioInputPinInit(&intAccelPin, 1);
    
    FXOS_ReadReg(&fxosHandle, TRANSIENT_SRC_REG, tmp, 1);
    
    if((tmp[0] & TEA_MASK)) 
    {
        if((tmp[0] & XTRANSE_MASK))
        {
            mpfAccelFunction(gAccel_X_c);
        }
        else if((tmp[0] & YTRANSE_MASK))
        {
            mpfAccelFunction(gAccel_Y_c);
        }
        else if((tmp[0] & ZTRANSE_MASK))
        {
            mpfAccelFunction(gAccel_Z_c);
        }
    }
    
    TMR_StartSingleShotTimer(mAccelTimerID, TIMER_200ms, (pfTmrCallBack_t)Accel_Interrupt_Enable, NULL);
}
#endif
/*!*************************************************************************************************
\fn     void* App_GetAccelDataString(void)
\brief  Return post data.

\param  [in]    none

\return         return data to be send through post
***************************************************************************************************/
void* App_GetAccelDataString
(
    uint8_t accelEvent
)
{
#if USE_ACCELEROMETER
    /* Static allocaction for accel string */
    static uint8_t sendAccelerationData [ACCEL_BUFF_SIZE] = {0};
    
    /* Accel strings */
    const uint8_t sX[] = "X= ";
    const uint8_t sY[] = "Y= ";
    const uint8_t sZ[] = "Z= ";
    const uint8_t sUnknown[] = "Unknown parameter";

    /* Compute accel */    
    uint16_t xData = 0;
    uint16_t yData = 0;
    uint16_t zData = 0;
    fxos_data_t sensorData = {0};
    
    uint8_t* pIndex = NULL;
    
    /* Clear data and reset buffers */
    FLib_MemSet(sendAccelerationData, 0, ACCEL_BUFF_SIZE);
    
    /* Compute output */
    /* Get new accelerometer data. */
    if (FXOS_ReadSensorData(&fxosHandle, &sensorData) != kStatus_Success)
    {
        return NULL;
    }

    /* Get the X and Y data from the sensor data structure in 14 bit left format data*/
    xData = ((uint16_t)((uint16_t)sensorData.accelXMSB << 8) | (uint16_t)sensorData.accelXLSB);
    yData = ((uint16_t)((uint16_t)sensorData.accelYMSB << 8) | (uint16_t)sensorData.accelYLSB);
    zData = ((uint16_t)((uint16_t)sensorData.accelZMSB << 8) | (uint16_t)sensorData.accelZLSB);
        
    switch(accelEvent)
    {
        case gAccel_X_c:
            pIndex = sendAccelerationData;
            FLib_MemCpy(pIndex, (void *)sX, SizeOfString(sX));
            pIndex += SizeOfString(sX);
            NWKU_PrintDec(xData, pIndex, 5, FALSE);
            break;
            
        case gAccel_Y_c:
            pIndex = sendAccelerationData;
            FLib_MemCpy(pIndex, (void *)sY, SizeOfString(sY));
            pIndex += SizeOfString(sY);
            NWKU_PrintDec(yData, pIndex, 5, FALSE);
            break;
            
        case gAccel_Z_c:
            pIndex = sendAccelerationData;
            FLib_MemCpy(pIndex, (void *)sZ, SizeOfString(sZ));
            pIndex += SizeOfString(sZ);
            NWKU_PrintDec(zData, pIndex, 5, FALSE);
            break;
            
        case gAccel_All_c:
            pIndex = sendAccelerationData;
            FLib_MemCpy(pIndex, (void *)sX, SizeOfString(sX));
            pIndex += SizeOfString(sX);
            NWKU_PrintDec(xData, pIndex, 5, FALSE);
            pIndex += (xData < 10 ? 1 : (xData < 100 ? 2 : (xData < 1000 ? 3 : (xData < 10000 ? 4 : 5))));
            *pIndex++ = ',';
            *pIndex++ = ' ';
            FLib_MemCpy(pIndex, (void *)sY, SizeOfString(sY));
            pIndex += SizeOfString(sY);
            NWKU_PrintDec(yData, pIndex, 5, FALSE);
            pIndex += (yData < 10 ? 1 : (yData < 100 ? 2 : (yData < 1000 ? 3 : (yData < 10000 ? 4 : 5))));
            *pIndex++ = ',';
            *pIndex++ = ' ';
            FLib_MemCpy(pIndex, (void *)sZ, SizeOfString(sZ));
            pIndex += SizeOfString(sZ);
            NWKU_PrintDec(zData, pIndex, 5, FALSE);
            break;
            
        default:
            FLib_MemCpy(sendAccelerationData, (void *) sUnknown, SizeOfString(sUnknown));
            break;
    }   
    
    return sendAccelerationData;
#else
    return NULL;
#endif    
}

#if USE_ACCELEROMETER
/*==================================================================================================
Private functions
==================================================================================================*/
/*!*************************************************************************************************
\fn             static void Accel_Interrupt_Enable(void)
\brief          Callback triggered on the ISR.

It will enable the accelerometer interrupt pin.

\return         void
***************************************************************************************************/
static void Accel_Interrupt_Enable(void)
{
    uint8_t tmp[1] = {0};
    
    intAccelPin.interruptSelect = pinInt_FallingEdge_c;
    (void)GpioInputPinInit(&intAccelPin, 1);
    
    FXOS_ReadReg(&fxosHandle, TRANSIENT_SRC_REG, tmp, 1);
}
#endif /* USE_ACCELEROMETER */
// @}
/*===============================================================================================
Private debug functions
==================================================================================================*/

