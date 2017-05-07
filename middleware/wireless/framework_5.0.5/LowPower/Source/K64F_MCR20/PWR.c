/*!
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* All rights reserved.
*
* \file PWR.c
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

/*****************************************************************************
 *                               INCLUDED HEADERS                            *
 *---------------------------------------------------------------------------*
 * Add to this section all the headers that this module needs to include.    *
 *---------------------------------------------------------------------------*
 *****************************************************************************/
#include "EmbeddedTypes.h"
#include "PWRLib.h"
#include "PWR_Configuration.h"
#include "PWR_Interface.h"
#include "TimersManager.h"
#include "Phy.h"
#include "MCR20Drv.h"
#include "MCR20Reg.h"
#include "Keyboard.h"
#include "SerialManager.h"

#include "fsl_lptmr.h"
#include "fsl_smc.h"
#include "fsl_clock.h"
#include "fsl_os_abstraction.h"


/*****************************************************************************
 *                             PRIVATE MACROS                                *
 *---------------------------------------------------------------------------*
 * Add to this section all the access macros, registers mappings, bit access *
 * macros, masks, flags etc ...                                              *
 *---------------------------------------------------------------------------*
 *****************************************************************************/

/* Minimum sleep ticks (16us) in DeepSleepMode 13  */
#define PWR_MINIMUM_SLEEP_TICKS   10
#define gPhyTimerFreq_c           62500
#define gRTCOscFreq_c             32768
#define gClockConfig_FEI_24_c   CPU_CLOCK_CONFIG_0
#define gClockConfig_PEE_48_c   CPU_CLOCK_CONFIG_1
#define gClockConfig_BLPI_4_c   CPU_CLOCK_CONFIG_2

/*****************************************************************************
 *                               PRIVATE VARIABLES                           *
 *---------------------------------------------------------------------------*
 * Add to this section all the variables and constants that have local       *
 * (file) scope.                                                             *
 * Each of this declarations shall be preceded by the 'static' keyword.      *
 * These variables / constants cannot be accessed outside this module.       *
 *---------------------------------------------------------------------------*
 *****************************************************************************/
uint8_t mLPMFlag = gAllowDeviceToSleep_c;
uint8_t mLpmXcvrDisallowCnt = 0;

#if (cPWR_UsePowerDownMode)
static uint32_t mPWR_DeepSleepTime = cPWR_DeepSleepDurationMs;
#endif //(cPWR_UsePowerDownMode)

/*****************************************************************************
 *                               PUBLIC VARIABLES                            *
 *---------------------------------------------------------------------------*
 * Add to this section all the variables and constants that have global      *
 * (project) scope.                                                          *
 * These variables / constants can be accessed outside this module.          *
 * These variables / constants shall be preceded by the 'extern' keyword in  *
 * the interface header.                                                     *
 *---------------------------------------------------------------------------*
 *****************************************************************************/

/*****************************************************************************
 *                           PRIVATE FUNCTIONS PROTOTYPES                    *
 *---------------------------------------------------------------------------*
 * Add to this section all the functions prototypes that have local (file)   *
 * scope.                                                                    *
 * These functions cannot be accessed outside this module.                   *
 * These declarations shall be preceded by the 'static' keyword.             *
 *---------------------------------------------------------------------------*
 *****************************************************************************/
typedef enum
{
  PWR_Run = 77,
  PWR_Sleep,
  PWR_DeepSleep,
  PWR_Reset,
  PWR_OFF
} PWR_CheckForAndEnterNewPowerState_t;
/*****************************************************************************
 *                             PRIVATE FUNCTIONS                             *
 *---------------------------------------------------------------------------*
 * Add to this section all the functions that have local (file) scope.       *
 * These functions cannot be accessed outside this module.                   *
 * These definitions shall be preceded by the 'static' keyword.              *
 *---------------------------------------------------------------------------*
 *****************************************************************************/

/*****************************************************************************
 *                             PUBLIC FUNCTIONS                              *
 *---------------------------------------------------------------------------*
 * Add to this section all the functions that have global (project) scope.   *
 * These functions can be accessed outside this module.                      *
 * These functions shall have their declarations (prototypes) within the     *
 * interface header file and shall be preceded by the 'extern' keyword.      *
 *---------------------------------------------------------------------------*
 *****************************************************************************/

/*---------------------------------------------------------------------------
 * Name: PWR_Init
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWR_Init
(
  void
)
{
#if (cPWR_UsePowerDownMode)

  PWRLib_Init();

#endif  /* #if (cPWR_UsePowerDownMode) */
}

/*---------------------------------------------------------------------------
 * Name: PWR_SetDeepSleepTimeInMs
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWR_SetDeepSleepTimeInMs
(
  uint32_t deepSleepTimeTimeMs
)
{
#if (cPWR_UsePowerDownMode)
 if(deepSleepTimeTimeMs == 0)
 {
  return;
 }
 mPWR_DeepSleepTime = deepSleepTimeTimeMs;
#else
 (void) deepSleepTimeTimeMs;
#endif
}

/*---------------------------------------------------------------------------
 * Name: PWR_SetDeepSleepTimeInSymbols
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWR_SetDeepSleepTimeInSymbols
(
  uint32_t deepSleepTimeTimeSym
)
{
#if (cPWR_UsePowerDownMode)
    if(deepSleepTimeTimeSym == 0)
    {
        return;
    }
    mPWR_DeepSleepTime = (deepSleepTimeTimeSym*16)/1000; 
#else
    (void) deepSleepTimeTimeSym;
#endif
}

/*---------------------------------------------------------------------------
 * Name: PWR_AllowDeviceToSleep
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWR_AllowDeviceToSleep
(
void
)
{
  OSA_DisableIRQGlobal();

  if( mLPMFlag != 0 ){
    mLPMFlag--;
  }
  OSA_EnableIRQGlobal();
}

/*---------------------------------------------------------------------------
 * Name: PWR_DisallowDeviceToSleep
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWR_DisallowDeviceToSleep
(
void
)
{
  uint8_t prot;
  OSA_DisableIRQGlobal();
  prot = mLPMFlag + 1;
  if(prot != 0)
  {
    mLPMFlag++;
  }
  OSA_EnableIRQGlobal();
}

/*---------------------------------------------------------------------------
 * Name: PWR_AllowXcvrToSleep
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWR_AllowXcvrToSleep(void)
{
#if (cPWR_UsePowerDownMode)
    OSA_DisableIRQGlobal();
    
    if( mLpmXcvrDisallowCnt != 0 )
    {
        mLpmXcvrDisallowCnt--;
    }

    OSA_EnableIRQGlobal();
#endif
}

/*---------------------------------------------------------------------------
 * Name: PWR_DisallowXcvrToSleep
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWR_DisallowXcvrToSleep(void)
{
#if (cPWR_UsePowerDownMode)
    uint8_t prot;
    
    OSA_DisableIRQGlobal();
    
    prot = mLpmXcvrDisallowCnt + 1;
    
    if(prot != 0)
    {
        mLpmXcvrDisallowCnt++;
    }
    
    OSA_EnableIRQGlobal();
#endif
}

/*---------------------------------------------------------------------------
 * Name: PWR_CheckIfDeviceCanGoToSleep
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
bool_t PWR_CheckIfDeviceCanGoToSleep
(
void
)
{
  bool_t   returnValue;
  OSA_DisableIRQGlobal();
  returnValue = mLPMFlag == 0 ? TRUE : FALSE;
  OSA_EnableIRQGlobal();
  return returnValue;
}

/*---------------------------------------------------------------------------
 * Name: PWR_SleepAllowed
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
bool_t PWR_SleepAllowed
(
void
)
{
#if (cPWR_UsePowerDownMode)
    if((PWRLib_GetCurrentZigbeeStackPowerState == StackPS_Sleep) ||  \
        (PWRLib_GetCurrentZigbeeStackPowerState == StackPS_DeepSleep) )
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
#else
    return TRUE;
#endif  /* #if (cPWR_UsePowerDownMode) else */
}

/*---------------------------------------------------------------------------
 * Name: PWR_SleepAllowed
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
bool_t PWR_DeepSleepAllowed
(
void
)
{
    bool_t state = TRUE;
#if (cPWR_UsePowerDownMode)
    if (PWRLib_GetCurrentZigbeeStackPowerState == StackPS_DeepSleep) 
    {
#if (cPWR_DeepSleepMode != 13)
        /* DeepSleepMode 13 allows the radio to be active during low power */
        if( mLpmXcvrDisallowCnt )
        {
            state = FALSE;
        }
#endif
    }
    else
    {
        state = FALSE;
    }
#endif  /* #if (cPWR_UsePowerDownMode)*/
    return state;
}

/*---------------------------------------------------------------------------
 * Name: PWR_HandleDeepSleep
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
#if (cPWR_UsePowerDownMode)
static PWRLib_WakeupReason_t PWR_HandleDeepSleep
(
void
)
{
    PWRLib_WakeupReason_t  Res;
    uint32_t sleepTimeMs;
    uint32_t lptmrTicks;
    uint32_t lptmrFreq;
    uint8_t clkMode;
    
#if ( (cPWR_DeepSleepMode == 9) || (cPWR_DeepSleepMode == 10) || (cPWR_DeepSleepMode == 11) || (cPWR_DeepSleepMode == 12) || (cPWR_DeepSleepMode == 13))
#if (gTMR_EnableLowPowerTimers_d)
    uint32_t notCountedTicksBeforeSleep = 0;
#endif
#endif
    
#if (cPWR_UsePowerDownMode)
    Res.AllBits = 0;
    PWRLib_MCU_WakeupReason.AllBits = 0;

#if (gTMR_EnableLowPowerTimers_d) && (cPWR_CheckLowPowerTimers)
    /* Get the expire time of the first programmed Low Power Timer */
    sleepTimeMs = TMR_GetFirstExpireTime(gTmrLowPowerTimer_c);
    if( mPWR_DeepSleepTime < sleepTimeMs )
#endif
    {
        sleepTimeMs = mPWR_DeepSleepTime;
    }
    PWRLib_LPTMR_GetTimeSettings(sleepTimeMs ,&clkMode ,&lptmrTicks, &lptmrFreq);

  /*---------------------------------------------------------------------------*/
#if (cPWR_DeepSleepMode == 0)
  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 1)
  /* configure MCG in FLL Engaged Internal (FEI) mode */
  BOARD_EnterLowPowerCb();
  /* configure Radio in hibernate mode */
  PWRLib_Radio_Enter_Hibernate();
  /* configure MCU in VLLS2 low power mode */
  PWRLib_MCU_Enter_VLLS2();
  /* never returns. VLLSx wakeup goes through Reset sequence. */
  //but
  //  If an interrupt configured to wake up the MCU from VLLS occurs before or
  //  during the VLLS entry sequence it prevents the system from entering low power
  //  and bit STOPA from SMC_PMCTRL becomes set. In this case the function returns
  //  the reasons that prevent it to enter low power.
  PWRLib_LLWU_UpdateWakeupReason();
  PWRLib_Radio_Enter_AutoDoze();
  /* configure MCG in PEE/FEE mode*/
  BOARD_ExitLowPowerCb();
  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 2)
#if (cPWR_LPTMRClockSource != cLPTMR_Source_Int_LPO_1KHz)
#error  "*** ERROR: cPWR_LPTMRClockSource has to be set to cLPTMR_Source_Int_LPO_1KHz"
#endif 
  //       /* configure MCG in FLL Engaged Internal (FEI) mode */
  BOARD_EnterLowPowerCb();
  /* configure Radio in hibernate mode */
  PWRLib_Radio_Enter_Hibernate();
  /* start LPTMR */
  PWRLib_LPTMR_ClockStart(clkMode,lptmrTicks);

  /* configure MCU in VLLS2 low power mode */
  PWRLib_MCU_Enter_VLLS2();
  /* never returns. VLLSx wakeup goes through Reset sequence. */
  //but
  //  If an interrupt configured to wake up the MCU from VLLS occurs before or
  //  during the VLLS entry sequence it prevents the system from entering low power
  //  and bit STOPA from SMC_PMCTRL becomes set. In this case the function returns
  //  the reasons that prevent it to enter low power.
  PWRLib_LLWU_UpdateWakeupReason();
  PWRLib_LPTMR_ClockStop();
  PWRLib_Radio_Enter_AutoDoze();
  /* configure MCG in PEE/FEE mode*/
  BOARD_ExitLowPowerCb();
  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 3)
#if (cPWR_LPTMRClockSource != cLPTMR_Source_Ext_ERCLK32K)
#error  "*** ERROR: cPWR_LPTMRClockSource has to be set to cLPTMR_Source_Ext_ERCLK32K"
#endif
  /* configure MCG in FLL Engaged Internal (FEI) mode */
  BOARD_EnterLowPowerCb();
  /* configure Radio in hibernate mode */
  PWRLib_Radio_Enter_Hibernate();
  /* start LPTMR */
  PWRLib_LPTMR_ClockStart(clkMode,lptmrTicks);
  /* configure MCU in VLLS2 low power mode */
  PWRLib_MCU_Enter_VLLS1();
  /* never returns. VLLSx wakeup goes through Reset sequence. */
  //but
  //  If an interrupt configured to wake up the MCU from VLLS occurs before or
  //  during the VLLS entry sequence it prevents the system from entering low power
  //  and bit STOPA from SMC_PMCTRL becomes set. In this case the function returns
  //  the reasons that prevent it to enter low power.
  PWRLib_LLWU_UpdateWakeupReason();
  PWRLib_LPTMR_ClockStop();
  PWRLib_Radio_Enter_AutoDoze();
  /* configure MCG in PEE/FEE mode*/
  BOARD_ExitLowPowerCb();

  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 4)
#if (!gTimestamp_Enabled_d)
#error "*** ERROR: gTimestamp_Enabled_d has to be set TRUE"
#endif
  /* configure MCG in FLL Engaged Internal (FEI) mode */
  BOARD_EnterLowPowerCb();
  /* configure Radio in hibernate mode */
  PWRLib_Radio_Enter_Hibernate();
  TMR_RTCSetAlarmRelative(sleepTimeMs/1000, NULL, NULL);
  /* configure MCU in VLLS2 low power mode */
  PWRLib_MCU_Enter_VLLS2();
  /* never returns. VLLSx wakeup goes through Reset sequence. */
  //but
  //  If an interrupt configured to wake up the MCU from VLLS occurs before or
  //  during the VLLS entry sequence it prevents the system from entering low power
  //  and bit STOPA from SMC_PMCTRL becomes set. In this case the function returns
  //  the reasons that prevent it to enter low power.
  PWRLib_LLWU_UpdateWakeupReason();
  PWRLib_Radio_Enter_AutoDoze();
  /* configure MCG in PEE/FEE mode*/
  BOARD_ExitLowPowerCb();
  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 5)
  
#if (cPWR_LPTMRClockSource != cLPTMR_Source_Int_LPO_1KHz)
#error  "*** ERROR: cPWR_LPTMRClockSource has to be set to cLPTMR_Source_Int_LPO_1KHz"
#endif 
  /* configure MCG in FLL Engaged Internal (FEI) mode */
  BOARD_EnterLowPowerCb();
  /* configure Radio in hibernate mode */
  PWRLib_Radio_Enter_Hibernate();
  /* start LPTMR */
  PWRLib_LPTMR_ClockStart(clkMode,lptmrTicks);

  /* configure MCU in VLLS2 low power mode */
  PWRLib_MCU_Enter_VLLS2();
  /* never returns. VLLSx wakeup goes through Reset sequence. */
  //but
  //  If an interrupt configured to wake up the MCU from VLLS occurs before or
  //  during the VLLS entry sequence it prevents the system from entering low power
  //  and bit STOPA from SMC_PMCTRL becomes set. In this case the function returns
  //  the reasons that prevent it to enter low power.
  PWRLib_LLWU_UpdateWakeupReason();
  PWRLib_LPTMR_ClockStop();
  PWRLib_Radio_Enter_AutoDoze();
  /* configure MCG in PEE/FEE mode*/
  BOARD_ExitLowPowerCb();
  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 6)
#if (cPWR_LPTMRClockSource != cLPTMR_Source_Ext_ERCLK32K)
#error  "*** ERROR: cPWR_LPTMRClockSource has to be set to cLPTMR_Source_Ext_ERCLK32K"
#endif 
  BOARD_EnterLowPowerCb();
  /* configure Radio in hibernate mode */
  PWRLib_Radio_Enter_Hibernate();
  /* start LPTMR */
  PWRLib_LPTMR_ClockStart(clkMode,lptmrTicks);

  /* configure MCU in VLLS2 low power mode */
  PWRLib_MCU_Enter_VLLS2();
  /* never returns. VLLSx wakeup goes through Reset sequence. */
  //but
  //  If an interrupt configured to wake up the MCU from VLLS occurs before or
  //  during the VLLS entry sequence it prevents the system from entering low power
  //  and bit STOPA from SMC_PMCTRL becomes set. In this case the function returns
  //  the reasons that prevent it to enter low power.
  PWRLib_LLWU_UpdateWakeupReason();
  PWRLib_LPTMR_ClockStop();
  PWRLib_Radio_Enter_AutoDoze();
  /* configure MCG in PEE/FEE mode*/
  BOARD_ExitLowPowerCb();
  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 7)
#if (!gTimestamp_Enabled_d)
#error "*** ERROR: gTimestamp_Enabled_d has to be set TRUE"
#endif /* #if (!gTimestamp_Enabled_d) */           
  
  /* configure MCG in FLL Engaged Internal (FEI) mode */
  BOARD_EnterLowPowerCb();
  /* configure Radio in hibernate mode */
  PWRLib_Radio_Enter_Hibernate();
  TMR_RTCSetAlarmRelative(sleepTimeMs/1000, NULL, NULL);

  /* configure MCU in VLLS2 low power mode */
  PWRLib_MCU_Enter_VLLS2();
  /* never returns. VLLSx wakeup goes through Reset sequence. */
  //but
  //  If an interrupt configured to wake up the MCU from VLLS occurs before or
  //  during the VLLS entry sequence it prevents the system from entering low power
  //  and bit STOPA from SMC_PMCTRL becomes set. In this case the function returns
  //  the reasons that prevent it to enter low power.
  PWRLib_LLWU_UpdateWakeupReason(); 
  PWRLib_Radio_Enter_AutoDoze();
  /* configure MCG in PEE/FEE mode*/
  BOARD_ExitLowPowerCb();
  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 8)

#if (cPWR_LPTMRClockSource != cLPTMR_Source_Int_LPO_1KHz)
#error  "*** ERROR: cPWR_LPTMRClockSource has to be set to cLPTMR_Source_Int_LPO_1KHz"
#endif
  /* configure MCG in FLL Engaged Internal (FEI) mode */
  BOARD_EnterLowPowerCb();
  /* configure Radio in hibernate mode */
  PWRLib_Radio_Enter_Hibernate();
  /* start LPTMR */
  PWRLib_LPTMR_ClockStart(clkMode,lptmrTicks);
  /* configure MCU in VLLS2 low power mode */
  PWRLib_MCU_Enter_VLLS2();
  /* never returns. VLLSx wakeup goes through Reset sequence. */
  //but
  //  If an interrupt configured to wake up the MCU from VLLS occurs before or
  //  during the VLLS entry sequence it prevents the system from entering low power
  //  and bit STOPA from SMC_PMCTRL becomes set. In this case the function returns
  //  the reasons that prevent it to enter low power.
  PWRLib_LLWU_UpdateWakeupReason();
  PWRLib_LPTMR_ClockStop();
  PWRLib_Radio_Enter_AutoDoze();
  /* configure MCG in PEE/FEE mode*/
  BOARD_ExitLowPowerCb();
  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 9)

#if (cPWR_LPTMRClockSource != cLPTMR_Source_Int_LPO_1KHz)
#error  "*** ERROR: cPWR_LPTMRClockSource has to be set to cLPTMR_Source_Int_LPO_1KHz"
#endif

#if (gTMR_EnableLowPowerTimers_d)
  /* if more low power timers are running, stop the hardware timer
  and save the spend time in ticks that wasn't counted.  */
  notCountedTicksBeforeSleep = TMR_NotCountedTicksBeforeSleep();
#endif
  /* This is the place where PWRLib_LPTMR_ClockStart should be called*/
  /* Unfortunately LPTMR is reset by CLOCK_SetMcgConfig()*/
  /* The duration of CLOCK_SetMcgConfig() will not be counted*/
  
  /* configure MCG in FLL Engaged Internal (FEI) mode */
  BOARD_EnterLowPowerCb();
  /* start LPTMR */
  PWRLib_LPTMR_ClockStart(clkMode,lptmrTicks);
  /* configure Radio in hibernate mode */
  PWRLib_Radio_Enter_Hibernate();
  /* configure MCU in LLS low power mode */
  PWRLib_MCU_Enter_LLS3();
  /* checks sources of wakeup */

  PWRLib_LLWU_UpdateWakeupReason();
  /* configure Radio in autodoze mode */
  PWRLib_Radio_Enter_AutoDoze();
  /* stop LPTMR */
  PWRLib_LPTMR_ClockStop();
  /* configure MCG in PLL Engaged External (PEE) mode */
  BOARD_ExitLowPowerCb();
  
  /* This is the place where PWRLib_LPTMR_ClockStop should be called*/
  /* Unfortunately LPTMR is reset by CLOCK_SetMcgConfig()*/
  /* The duration of CLOCK_SetMcgConfig() will not be counted*/
  /* Sync. the low power timers */
#if (gTMR_EnableLowPowerTimers_d)
    {
        uint64_t timerTicks;
        timerTicks = ((uint64_t)PWRLib_LPTMR_ClockCheck()*TMR_GetTimerFreq())/lptmrFreq;
        timerTicks += notCountedTicksBeforeSleep;
        TMR_SyncLpmTimers((uint32_t)timerTicks);
    }
#endif

  if(PWRLib_MCU_WakeupReason.Bits.DeepSleepTimeout == 1)
  {
    cPWR_DeepSleepWakeupStackProc; // User function called only on timeout
  }

  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 10)

#if (cPWR_LPTMRClockSource != cLPTMR_Source_Ext_ERCLK32K)
#error  "*** ERROR: cPWR_LPTMRClockSource has to be set to cLPTMR_Source_Ext_ERCLK32K"
#endif /* #if (cPWR_LPTMRClockSource == cLPTMR_Source_Int_LPO_1KHz) */


#if (gTMR_EnableLowPowerTimers_d)
  /* if more low power timers are running, stop the hardware timer
  and save the spend time in ticks that wasn't counted.  */
  notCountedTicksBeforeSleep = TMR_NotCountedTicksBeforeSleep();
#endif
  /* This is the place where PWRLib_LPTMR_ClockStart should be called*/
  /* Unfortunately LPTMR is reset by CLOCK_SetMcgConfig()*/
  /* The duration of CLOCK_SetMcgConfig() will not be counted*/
  
  /* configure MCG in FLL Engaged Internal (FEI) mode */
  BOARD_EnterLowPowerCb();
  /* start LPTMR */
  PWRLib_LPTMR_ClockStart(clkMode,lptmrTicks);
  /* configure Radio in hibernate mode */
  PWRLib_Radio_Enter_Hibernate();
  /* configure MCU in LLS low power mode */
  PWRLib_MCU_Enter_LLS3();

  /* checks sources of wakeup */
  PWRLib_LLWU_UpdateWakeupReason();
  /* configure Radio in autodoze mode */
  PWRLib_Radio_Enter_AutoDoze();
  /* stop LPTMR */
  PWRLib_LPTMR_ClockStop();
  /* configure MCG in PEE/FEE mode*/
  BOARD_ExitLowPowerCb();
  /* This is the place where PWRLib_LPTMR_ClockStop should be called*/
  /* Unfortunately LPTMR is reset by CLOCK_SetMcgConfig()*/
  /* The duration of CLOCK_SetMcgConfig() will not be counted*/

  /* Sync. the low power timers */
#if (gTMR_EnableLowPowerTimers_d)
    {
        uint64_t timerTicks;
        timerTicks = ((uint64_t)PWRLib_LPTMR_ClockCheck()*TMR_GetTimerFreq())/lptmrFreq;
        timerTicks += notCountedTicksBeforeSleep;
        TMR_SyncLpmTimers((uint32_t)timerTicks);
    }
#endif


  if( PWRLib_MCU_WakeupReason.Bits.DeepSleepTimeout)
  {
    cPWR_DeepSleepWakeupStackProc; // User function called only on timeout
  }

  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 11)
#if (!gTimestamp_Enabled_d)
#error "*** ERROR: gTimestamp_Enabled_d has to be set TRUE"
#endif /* #if (!gTimestamp_Enabled_d) */
  {

#if (gTMR_EnableLowPowerTimers_d)
    uint64_t timeStamp;
    /* if more low power timers are running, stop the hardware timer
    and save the spend time in ticks that wasn't counted.  */
    notCountedTicksBeforeSleep = TMR_NotCountedTicksBeforeSleep();
    timeStamp = TMR_RTCGetTimestamp();
#endif
    /* configure MCG in FLL Engaged Internal (FEI) mode */
    BOARD_EnterLowPowerCb();
    /* configure Radio in hibernate mode */
    PWRLib_Radio_Enter_Hibernate();
    TMR_RTCSetAlarmRelative(sleepTimeMs/1000, NULL, NULL);
    /* configure MCU in LLS low power mode */
    PWRLib_MCU_Enter_LLS3();

    /* checks sources of wakeup */
    PWRLib_LLWU_UpdateWakeupReason();
    /* configure Radio in autodoze mode */
    PWRLib_Radio_Enter_AutoDoze();
    /* configure MCG in PEE/FEE mode*/
    BOARD_ExitLowPowerCb();
    /* Sync. the low power timers */
#if (gTMR_EnableLowPowerTimers_d)
    {
      uint64_t timerTicks = ((TMR_RTCGetTimestamp() - timeStamp)*TMR_GetTimerFreq())/1000000;
      timerTicks += notCountedTicksBeforeSleep;
      if(timerTicks > 0xffffffff)
      {
        timerTicks = 0xffffffff;
      }
      TMR_SyncLpmTimers((uint32_t)timerTicks);
    }
#endif
    if(PWRLib_MCU_WakeupReason.Bits.DeepSleepTimeout == 1)
    {
      cPWR_DeepSleepWakeupStackProc; // User function called only on timeout
    }
  }

  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 12)

#if (cPWR_LPTMRClockSource != cLPTMR_Source_Int_LPO_1KHz)
#error  "*** ERROR: cPWR_LPTMRClockSource has to be set to cLPTMR_Source_Int_LPO_1KHz"
#endif

#if (gTMR_EnableLowPowerTimers_d)
  /* if more low power timers are running, stop the hardware timer
  and save the spend time in ticks that wasn't counted.  */
  notCountedTicksBeforeSleep = TMR_NotCountedTicksBeforeSleep();
#endif

  /* This is the place where PWRLib_LPTMR_ClockStart should be called*/
  /* Unfortunately LPTMR is reset by CLOCK_SetMcgConfig()*/
  /* The duration of CLOCK_SetMcgConfig() will not be counted*/
  
  /* configure MCG in FLL Engaged Internal (FEI) mode */
  BOARD_EnterLowPowerCb();
  /* start LPTMR */
  PWRLib_LPTMR_ClockStart(clkMode,lptmrTicks);
  /* configure Radio in hibernate mode */
  PWRLib_Radio_Enter_Hibernate();
  /* configure MCU in LLS low power mode */
  PWRLib_MCU_Enter_LLS3();

  /* checks sources of wakeup */
  PWRLib_LLWU_UpdateWakeupReason();
  /* configure Radio in autodoze mode */
  PWRLib_Radio_Enter_AutoDoze();
  /* stop LPTMR */
  PWRLib_LPTMR_ClockStop();
  /* configure MCG in PEE/FEE mode*/
  BOARD_ExitLowPowerCb();
  /* This is the place where PWRLib_LPTMR_ClockStop should be called*/
  /* Unfortunately LPTMR is reset by CLOCK_SetMcgConfig()*/
  /* The duration of CLOCK_SetMcgConfig() will not be counted*/

  /* Sync. the low power timers */
#if (gTMR_EnableLowPowerTimers_d)
    {
        uint64_t timerTicks;
        timerTicks = ((uint64_t)PWRLib_LPTMR_ClockCheck()*TMR_GetTimerFreq())/lptmrFreq;
        timerTicks += notCountedTicksBeforeSleep;
        TMR_SyncLpmTimers((uint32_t)timerTicks);
    }
#endif
  if(PWRLib_MCU_WakeupReason.Bits.DeepSleepTimeout == 1)
  {
    cPWR_DeepSleepWakeupStackProc; // User function called only on timeout
  }

  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 13)
#if (!gKeyBoardSupported_d)
#error "*** ERROR: gKeyBoardSupported_d has to be set to TRUE"
#endif

#if ( gSerialMgrUseUart_c  == 0 )
#error "*** ERROR: gSerialMgrUseUart_c has to be set to TRUE"
#endif
  {
    uint32_t deepSleepTicks = 0;
    /* converts deep sleep duration from ms to symbols */
    deepSleepTicks = ( ( ( mPWR_DeepSleepTime / 2 ) * 125 ) + ( ( mPWR_DeepSleepTime & 1 ) * 62 ) ) & 0xFFFFFF;

    if( deepSleepTicks > PWR_MINIMUM_SLEEP_TICKS )
    {
#if (gTMR_EnableLowPowerTimers_d)
      uint64_t lptSyncTime; 
#endif
      uint64_t currentTime;
      uint32_t absoluteWakeUpTime;
      uint32_t temp;

#if (gTMR_EnableLowPowerTimers_d)
      /* if more low power timers are running, stop the hardware timer
      and save the spend time in ticks that wasn't counted.  */
      notCountedTicksBeforeSleep = TMR_NotCountedTicksBeforeSleep();
      PhyTimeReadClock(&lptSyncTime);
#endif

      /* configure MCG in FLL Engaged Internal (BLPI) mode */
      BOARD_EnterLowPowerCb();
      /* disable SysTick counter and interrupt */
      temp = SysTick->CTRL & (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);
      SysTick->CTRL &= ~(SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);
      /* disable transceiver CLK_OUT. */
      MCR20Drv_Set_CLK_OUT_Freq(gCLK_OUT_FREQ_DISABLE);
      /* configure Radio in Doze mode */
      PWRLib_Radio_Enter_Doze();
      /* prepare UART for low power operation */
      (void)Serial_EnableLowPowerWakeup(gSerialMgrUart_c);
      /* read current time */
      PhyTimeReadClock(&currentTime);
      /* compute absolute end time */
      absoluteWakeUpTime = (uint32_t)((currentTime + deepSleepTicks) & 0xFFFFFF);
      /* set absolute wakeup time */
      PhyTimeSetWakeUpTime(&absoluteWakeUpTime);

      /* configure MCU in VLPS low power mode */
      PWRLib_MCU_Enter_VLPS();
      /* checks sources of wakeup */
      /* radio timer wakeup */
      if( PhyTimeIsWakeUpTimeExpired() )
      {
        PWRLib_MCU_WakeupReason.Bits.DeepSleepTimeout = 1;     /* Sleep timeout ran out */
        PWRLib_MCU_WakeupReason.Bits.FromTimer = 1;            /* Wakeup by radio timer */
      }

      /*Check  UART module wakeup */
      if(Serial_IsWakeUpSource(gSerialMgrUart_c))
      {
        PWRLib_MCU_WakeupReason.Bits.FromUART = 1;
      }
      (void)Serial_DisableLowPowerWakeup( gSerialMgrUart_c);
      /* configure Radio in autodoze mode */
      PWRLib_Radio_Enter_AutoDoze();
      MCR20Drv_Set_CLK_OUT_Freq(gMCR20_ClkOutFreq_d);
      /* configure MCG in PEE/FEE mode*/
      BOARD_ExitLowPowerCb();
      /* restore the state of SysTick */
      SysTick->CTRL |= temp;

#if (gTMR_EnableLowPowerTimers_d)
      {
        uint64_t timerTicks;
        PhyTimeReadClock((phyTime_t*)&absoluteWakeUpTime);
        /* Converts the DozeDuration from radio ticks in software timer ticks and synchronize low power timers */
        timerTicks = (((absoluteWakeUpTime - lptSyncTime)& 0xFFFFFF)*TMR_GetTimerFreq())/gPhyTimerFreq_c;
        timerTicks += notCountedTicksBeforeSleep;
        if(timerTicks > 0xffffffff)
        {
          timerTicks = 0xffffffff;
        }
        TMR_SyncLpmTimers( (uint32_t)timerTicks );
      }
#endif
    }
    else
    {
      /* Not enough time to program the TMR compare */
      PWRLib_MCU_WakeupReason.Bits.DeepSleepTimeout = 1;     /* Sleep timeout ran out */
    }
    if(Res.Bits.DeepSleepTimeout == 1)
    {
      cPWR_DeepSleepWakeupStackProc; // User function called only on timeout
    }
  }

    /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 14)
#if (!gKeyBoardSupported_d)
#error "*** ERROR: gKeyBoardSupported_d has to be set to TRUE"
#endif

    /* configure MCG in FLL Engaged Internal (FEI) mode */
    BOARD_EnterLowPowerCb();
    /* configure Radio in hibernate mode */
    PWRLib_Radio_Enter_Hibernate();
    
    #if gPWR_EnsureOscStabilized_d
    /* start 32KHz OSC */
    while(PWRLib_RTC_IsOscStarted() == FALSE){}
    #endif
    /* configure MCU in LLS low power mode */
    PWRLib_MCU_Enter_LLS3();

    /* checks sources of wakeup */
    PWRLib_LLWU_UpdateWakeupReason();
    /* configure Radio in autodoze mode */
    PWRLib_Radio_Enter_AutoDoze();
    /* configure MCG in PEE/FEE mode*/
    BOARD_ExitLowPowerCb();

  /*---------------------------------------------------------------------------*/
#elif (cPWR_DeepSleepMode == 15)
#if (!gKeyBoardSupported_d)
#error "*** ERROR: gKeyBoardSupported_d has to be set to TRUE"
#endif

    /* configure MCU in LLS low power mode */
    PWRLib_MCU_Enter_LLS3();

    /* checks sources of wakeup */
    PWRLib_LLWU_UpdateWakeupReason();

  /*---------------------------------------------------------------------------*/
#else
#error "*** ERROR: Not a valid cPWR_DeepSleepMode chosen"
#endif

    Res.AllBits = PWRLib_MCU_WakeupReason.AllBits;
    return Res;
    
#else  /* #if (cPWR_UsePowerDownMode) else */
    
    /* to avoid unused warning*/
    Res.AllBits = 0xff;
    PWRLib_MCU_WakeupReason.AllBits = 0;
    return Res;
#endif  /* #if (cPWR_UsePowerDownMode) end */
}
#endif /* cPWR_UsePowerDownMode */

/*---------------------------------------------------------------------------
 * Name: PWR_HandleSleep
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
PWRLib_WakeupReason_t PWR_HandleSleep
(
void
)
{
  PWRLib_WakeupReason_t  Res;

  Res.AllBits = 0;

#if (cPWR_UsePowerDownMode)
  /*---------------------------------------------------------------------------*/
#if (cPWR_SleepMode==0)
  return Res;

  /*---------------------------------------------------------------------------*/
#elif (cPWR_SleepMode==1)
  /* radio in autodoze mode by default. mcu in wait mode */
  PWRLib_MCU_WakeupReason.AllBits = 0;
  PWRLib_MCU_Enter_Sleep();
  Res.Bits.SleepTimeout = 1;
  PWRLib_MCU_WakeupReason.Bits.SleepTimeout = 1;
  return Res;
  /*---------------------------------------------------------------------------*/
#else
#error "*** ERROR: Not a valid cPWR_SleepMode chosen"
#endif
#else  /* #if (cPWR_UsePowerDownMode) else */
  /* Last part to avoid unused warning */
  PWRLib_MCU_WakeupReason.AllBits = 0;
  return Res;
#endif  /* #if (cPWR_UsePowerDownMode) end */
}

/*---------------------------------------------------------------------------
 * Name: PWR_CheckForAndEnterNewPowerState
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
PWRLib_WakeupReason_t PWR_CheckForAndEnterNewPowerState
(
PWR_CheckForAndEnterNewPowerState_t NewPowerState
)
{
  PWRLib_WakeupReason_t ReturnValue;
  ReturnValue.AllBits = 0;

#if (cPWR_UsePowerDownMode)
  if ( NewPowerState == PWR_Run)
  {
    /* ReturnValue = 0; */
  }
  else if( NewPowerState == PWR_OFF)
  {
    /* configure MCG in FLL Engaged Internal (FEI) mode */
    BOARD_EnterLowPowerCb();
    /* configure Radio in hibernate mode */
    PWRLib_Radio_Enter_Hibernate();
    /* disable all wake up sources */
    PWRLib_LLWU_DisableAllWakeupSources();
    /* configure MCU in VLLS1 mode */
    PWRLib_MCU_Enter_VLLS1();

    /* Never returns */
    for(;;){}

  }
  else if( NewPowerState == PWR_Reset)
  {
    /* Never returns */
    PWRLib_Reset();
  }

  else if(( NewPowerState == PWR_DeepSleep) && PWR_DeepSleepAllowed())
  {
    ReturnValue = PWR_HandleDeepSleep();
  }
  else if(( NewPowerState == PWR_Sleep) && PWR_SleepAllowed())
  {
    ReturnValue = PWR_HandleSleep();
  }
  else
  {
    /* ReturnValue = FALSE; */
  }
  /* Clear wakeup reason */

#else
  /* To remove warning for variabels in functioncall */
  (void)NewPowerState;
#endif  /* #if (cPWR_UsePowerDownMode) */

  return ReturnValue;
}

/*---------------------------------------------------------------------------
 * Name: PWR_EnterPowerOff
 * Description: - Radio on Reset, MCU on VLLS1
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWR_EnterPowerOff(void)
{
  OSA_DisableIRQGlobal();
  (void)PWR_CheckForAndEnterNewPowerState(PWR_OFF);
  OSA_EnableIRQGlobal();
}
/*---------------------------------------------------------------------------
 * Name: PWRLib_LVD_ReportLevel
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
PWRLib_LVD_VoltageLevel_t PWRLib_LVD_ReportLevel
(
void
)
{
    PWRLib_LVD_VoltageLevel_t   Level;
#if ((cPWR_LVD_Enable == 0) || (cPWR_LVD_Enable == 3))
    Level = PWR_ABOVE_LEVEL_3_0V;
#elif (cPWR_LVD_Enable==1)
    Level = PWRLib_LVD_CollectLevel();
#elif (cPWR_LVD_Enable==2)
    extern PWRLib_LVD_VoltageLevel_t  PWRLib_LVD_SavedLevel;
    Level = PWRLib_LVD_SavedLevel;
#else
#error "*** ERROR: Illegal value for cPWR_LVD_Enable"
#endif /* #if (cPWR_LVD_Enable) */
    return Level;
}

/*---------------------------------------------------------------------------
 * Name: PWR_EnterLowPower
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
PWRLib_WakeupReason_t PWR_EnterLowPower
(
void
)
{
  PWRLib_WakeupReason_t ReturnValue;
#if (gTMR_EnableLowPowerTimers_d)
  bool_t unlockTMRThread = FALSE;
#endif
  ReturnValue.AllBits = 0;

  if (PWRLib_LVD_ReportLevel() == PWR_LEVEL_CRITICAL)
  {
    /* Voltage <= 1.8V so enter power-off state - to disable false Tx'ing(void)*/
    ReturnValue = PWR_CheckForAndEnterNewPowerState(PWR_OFF);
  }

  /* disable irq's */
  OSA_DisableIRQGlobal();

  PWRLib_SetCurrentZigbeeStackPowerState(StackPS_DeepSleep);

  if (
      TMR_AreAllTimersOff()
#if ( (cPWR_DeepSleepMode == 3) || (cPWR_DeepSleepMode == 4) || (cPWR_DeepSleepMode == 6) || (cPWR_DeepSleepMode == 7) || (cPWR_DeepSleepMode == 10) || (cPWR_DeepSleepMode == 11) )
#if gPWR_EnsureOscStabilized_d
        && TMR_RTCIsOscStarted()
#endif
#endif
          )  /*No timer running*/
  {
    /* if power lib is enabled */
#if (cPWR_UsePowerDownMode)
    /* if Low Power Capability is enabled */
#if (gTMR_EnableLowPowerTimers_d)
    /* if more low power timers are running, stop the hardware timer
    and save the spend time in ticks that wasn't counted.
    */
    unlockTMRThread = TRUE;
#endif /* #if (gTMR_EnableLowPowerTimers_d)  */
#endif /* #if (cPWR_UsePowerDownMode)  */

    ReturnValue = PWR_CheckForAndEnterNewPowerState (PWR_DeepSleep);
  }
  else /*timers are running*/
  {
    ReturnValue = PWR_CheckForAndEnterNewPowerState (PWR_Sleep);
  }

  /* restore irq's if there is pending evens */
  OSA_EnableIRQGlobal();

#if (gTMR_EnableLowPowerTimers_d)
  if(unlockTMRThread)
  {
    TMR_MakeTMRThreadReady();
  }
#endif

  return ReturnValue;
}

/*---------------------------------------------------------------------------
 * Name: PWR_EnterSleep
 * Description: - 
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWR_EnterSleep(void)
{
    PWRLib_MCU_Enter_Sleep();
}

/*---------------------------------------------------------------------------
 * Name: PWR_GetDeepSleepMode
 * Description: - 
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
uint8_t PWR_GetDeepSleepMode(void)
{
    return cPWR_DeepSleepMode;
}