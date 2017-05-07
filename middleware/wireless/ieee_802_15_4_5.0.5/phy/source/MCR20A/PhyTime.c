/*!
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* All rights reserved.
*
* \file PhyTime.c
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


/*! *********************************************************************************
*************************************************************************************
* Include
*************************************************************************************
********************************************************************************** */
#include "EmbeddedTypes.h"
#include "fsl_os_abstraction.h"
#include "MCR20Drv.h"
#include "MCR20Reg.h"
#include "Phy.h"

#include "FunctionLib.h"


/*! *********************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
********************************************************************************** */
#define gPhyTimeMinSetupTime_c (10) /* [symbols] */


/*! *********************************************************************************
*************************************************************************************
* Public memory declarations
*************************************************************************************
********************************************************************************** */
void (*gpfPhyTimeNotify)(void) = NULL;


/*! *********************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
********************************************************************************** */
static phyTimeEvent_t  mPhyTimers[gMaxPhyTimers_c];
static phyTimeEvent_t *pNextEvent;
volatile phyTime_t     mPhySeqTimeout;
volatile uint64_t      gPhyTimerOverflow;
static uint8_t         mPhyActiveTimers;
#if gPhyUseReducedSpiAccess_d
/* Mirror XCVR control registers */
extern uint8_t mStatusAndControlRegs[9];
#endif


/*! *********************************************************************************
*************************************************************************************
* Private prototypes
*************************************************************************************
********************************************************************************** */
static void PhyTime_OverflowCB( uint32_t param );
static phyTimeEvent_t* PhyTime_GetNextEvent( void );


/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
* \brief  Sets the start time of a sequence
*
* \param[in]  startTime  the start time for a sequence
*
********************************************************************************** */
#if gPhyUseReducedSpiAccess_d
void PhyTimeSetEventTrigger(phyTime_t startTime)
{
    uint8_t phyReg;
    
    OSA_InterruptDisable();
    
    phyReg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
    phyReg &= 0xF0;                    /* do not change other IRQs status */
    phyReg |= (cIRQSTS3_TMR2MSK);      /* mask TMR2 interrupt */
    MCR20Drv_DirectAccessSPIWrite(IRQSTS3, phyReg);
    
    MCR20Drv_DirectAccessSPIMultiByteWrite(T2PRIMECMP_LSB, (uint8_t *) &startTime, 2);
    
    phyReg |= (cIRQSTS3_TMR2IRQ);      /* aknowledge TMR2 IRQ */
    MCR20Drv_DirectAccessSPIWrite(IRQSTS3, phyReg);
    
    /* TC2PRIME_EN must be enabled in PHY_CTRL4 register */
    mStatusAndControlRegs[PHY_CTRL1] |= cPHY_CTRL1_TMRTRIGEN;    /* enable autosequence start by TC2 match */
    MCR20Drv_DirectAccessSPIWrite(PHY_CTRL1, mStatusAndControlRegs[PHY_CTRL1]);
    
    OSA_InterruptEnable();
}
#else
void PhyTimeSetEventTrigger
(
phyTime_t startTime
)
{
  uint8_t phyReg, phyCtrl3Reg;

  OSA_InterruptDisable();

  phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
  phyReg |= cPHY_CTRL1_TMRTRIGEN;    /* enable autosequence start by TC2 match */
  MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, phyReg);

  phyCtrl3Reg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL3);
  phyCtrl3Reg &= ~(cPHY_CTRL3_TMR2CMP_EN); /* disable TMR2 compare */
  MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyCtrl3Reg);

  MCR20Drv_DirectAccessSPIMultiByteWrite( (uint8_t) T2PRIMECMP_LSB, (uint8_t *) &startTime, 2);

  phyReg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
  phyReg &= 0xF0;                     /* do not change other IRQs status */
  phyReg &= ~(cIRQSTS3_TMR2MSK);      /* unmask TMR2 interrupt */
  phyReg |= (cIRQSTS3_TMR2IRQ);       /* aknowledge TMR2 IRQ */
  MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, phyReg);

  /* TC2PRIME_EN must be enabled in PHY_CTRL4 register */
  phyCtrl3Reg |= cPHY_CTRL3_TMR2CMP_EN;   /* enable TMR2 compare */
  MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyCtrl3Reg);

  OSA_InterruptEnable();
}
#endif

/*! *********************************************************************************
* \brief  Disable the time trigger for a sequence.
*
* \remarks The sequence will start asap
*
********************************************************************************** */
void PhyTimeDisableEventTrigger
(
void
)
{
    uint8_t phyReg;

    ProtectFromMCR20Interrupt();
#if gPhyUseReducedSpiAccess_d
    mStatusAndControlRegs[PHY_CTRL1] &= ~(cPHY_CTRL1_TMRTRIGEN); /* disable autosequence start by TC2 match */
    MCR20Drv_DirectAccessSPIWrite(PHY_CTRL1, mStatusAndControlRegs[PHY_CTRL1]);
#else

    phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
    phyReg &= ~(cPHY_CTRL1_TMRTRIGEN); /* disable autosequence start by TC2 match */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, phyReg); 
    
    phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL3);
    phyReg &= ~(cPHY_CTRL3_TMR2CMP_EN); /* disable TMR2 compare */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyReg);
#endif

    phyReg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
    phyReg &= 0xF0;                    /* do not change other IRQs status */
    phyReg |= (cIRQSTS3_TMR2MSK);      /* mask TMR2 interrupt */
    phyReg |= (cIRQSTS3_TMR2IRQ);      /* aknowledge TMR2 IRQ */
    MCR20Drv_DirectAccessSPIWrite(IRQSTS3, phyReg);

    UnprotectFromMCR20Interrupt();
}

/*! *********************************************************************************
* \brief  Sets the timeout value for a sequence
*
* \param[in]  pEndTime the absolute time when a sequence should terminate
*
* \remarks If the sequence does not finish until the timeout, it will be aborted
*
********************************************************************************** */
#if gPhyUseReducedSpiAccess_d
void PhyTimeSetEventTimeout(phyTime_t *pEndTime)
{
    uint8_t phyReg;
    
#ifdef PHY_PARAMETERS_VALIDATION
    if(NULL == pEndTime)
    {
        return;
    }
#endif /* PHY_PARAMETERS_VALIDATION */
    
    OSA_InterruptDisable();
    
    phyReg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
    phyReg &= 0xF0;                    /* do not change IRQ status */
    phyReg |= (cIRQSTS3_TMR3MSK);      /* mask TMR3 interrupt */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, phyReg);
    
    mPhySeqTimeout = *pEndTime & 0x00FFFFFF;
    MCR20Drv_DirectAccessSPIMultiByteWrite( (uint8_t) T3CMP_LSB, (uint8_t *) pEndTime, 3);
    
    phyReg &= ~(cIRQSTS3_TMR3MSK);      /* unmask TMR3 interrupt */
    phyReg |= (cIRQSTS3_TMR3IRQ);       /* aknowledge TMR3 IRQ */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, phyReg);
    
    OSA_InterruptEnable();
}
#else
void PhyTimeSetEventTimeout
(
phyTime_t *pEndTime
)
{
    uint8_t phyReg, phyCtrl3Reg;
    
#ifdef PHY_PARAMETERS_VALIDATION
    if(NULL == pEndTime)
    {
        return;
    }
#endif /* PHY_PARAMETERS_VALIDATION */
    
    OSA_InterruptDisable();
    
    phyCtrl3Reg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL3);
    phyCtrl3Reg &= ~(cPHY_CTRL3_TMR3CMP_EN); /* disable TMR3 compare */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyCtrl3Reg);
    
    phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL4);
    phyReg |= cPHY_CTRL4_TC3TMOUT;     /* enable autosequence stop by TC3 match */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL4, phyReg);
    
    mPhySeqTimeout = *pEndTime & 0x00FFFFFF;
    MCR20Drv_DirectAccessSPIMultiByteWrite( (uint8_t) T3CMP_LSB, (uint8_t *) pEndTime, 3);
    
    phyReg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
    phyReg &= 0xF0;                     /* do not change IRQ status */
    //  phyReg &= ~(cIRQSTS3_TMR3MSK);      /* unmask TMR3 interrupt */
    phyReg |= (cIRQSTS3_TMR3IRQ);       /* aknowledge TMR3 IRQ */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, phyReg);
    
    phyCtrl3Reg |= cPHY_CTRL3_TMR3CMP_EN;   /* enable TMR3 compare */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyCtrl3Reg);
    
    OSA_InterruptEnable();
}
#endif

/*! *********************************************************************************
* \brief  Return the timeout value for the current sequence
*
* \return  uint32_t the timeout value
*
********************************************************************************** */
phyTime_t PhyTimeGetEventTimeout( void )
{
    return mPhySeqTimeout;
}

/*! *********************************************************************************
* \brief  Disables the sequence timeout
*
********************************************************************************** */
void PhyTimeDisableEventTimeout
(
void
)
{
    uint8_t phyReg;
    
    ProtectFromMCR20Interrupt();
    
#if !gPhyUseReducedSpiAccess_d
    phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL4);
    phyReg &= ~(cPHY_CTRL4_TC3TMOUT);  /* disable autosequence stop by TC3 match */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL4, phyReg);
    
    phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL3);
    phyReg &= ~(cPHY_CTRL3_TMR3CMP_EN); /* disable TMR3 compare */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyReg);
#endif
    /* aknowledge and mask TMR3 IRQ */
    phyReg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
    phyReg &= 0xF0;                     /* do not change IRQ status */
    phyReg |= cIRQSTS3_TMR3IRQ | cIRQSTS3_TMR3MSK;
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, phyReg);
    
    UnprotectFromMCR20Interrupt();
}

/*! *********************************************************************************
* \brief  Reads the absolute clock from the radio
*
* \param[out]  pRetClk pointer to a location where the current clock will be stored
*
********************************************************************************** */
void PhyTimeReadClock
(
phyTime_t *pRetClk
)
{
#ifdef PHY_PARAMETERS_VALIDATION
    if(NULL == pRetClk)
    {
        return;
    }
#endif /* PHY_PARAMETERS_VALIDATION */

  OSA_InterruptDisable();

  *pRetClk = 0;
  MCR20Drv_DirectAccessSPIMultiByteRead( (uint8_t) EVENT_TMR_LSB, (uint8_t *) pRetClk, 3);

  OSA_InterruptEnable();

}

/*! *********************************************************************************
* \brief  Initialize the Event Timer
*
* \param[in]  pAbsTime  pointer to the location where the new time is stored
*
********************************************************************************** */
void PhyTimeInitEventTimer
(
uint32_t *pAbsTime
)
{
    uint8_t phyCtrl4Reg;

#ifdef PHY_PARAMETERS_VALIDATION
    if(NULL == pAbsTime)
    {
        return;
    }
#endif /* PHY_PARAMETERS_VALIDATION */

    OSA_InterruptDisable();
#if gPhyUseReducedSpiAccess_d
    phyCtrl4Reg = mStatusAndControlRegs[PHY_CTRL4] | cPHY_CTRL4_TMRLOAD; /* self clearing bit */
#else
    phyCtrl4Reg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL4);
    phyCtrl4Reg |= cPHY_CTRL4_TMRLOAD; /* self clearing bit */
#endif

    MCR20Drv_DirectAccessSPIMultiByteWrite( (uint8_t) T1CMP_LSB, (uint8_t *) pAbsTime, 3);
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL4, phyCtrl4Reg);

    OSA_InterruptEnable();
}

/*! *********************************************************************************
* \brief  Set TMR1 timeout value
*
* \param[in]  pWaitTimeout the timeout value
*
********************************************************************************** */
#if gPhyUseReducedSpiAccess_d
void PhyTimeSetWaitTimeout(phyTime_t *pWaitTimeout)
{
    uint8_t irqSts3Reg;
    
    OSA_InterruptDisable();
    
    irqSts3Reg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
    irqSts3Reg &= 0xF0;                     /* do not change other IRQs status */
    irqSts3Reg |= (cIRQSTS3_TMR1MSK);       /* mask TMR1 IRQ */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, irqSts3Reg);
    
    MCR20Drv_DirectAccessSPIMultiByteWrite( (uint8_t) T1CMP_LSB, (uint8_t *) pWaitTimeout, 3);
    
    irqSts3Reg &= ~(cIRQSTS3_TMR1MSK);      /* unmask TMR1 interrupt */
    irqSts3Reg |= (cIRQSTS3_TMR1IRQ);       /* aknowledge TMR1 IRQ */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, irqSts3Reg);
    
    OSA_InterruptEnable();
}
#else
void PhyTimeSetWaitTimeout
(
phyTime_t *pWaitTimeout
)
{
    uint8_t phyCtrl3Reg, irqSts3Reg;
    
    OSA_InterruptDisable();
    
    phyCtrl3Reg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL3);
    phyCtrl3Reg &= ~(cPHY_CTRL3_TMR1CMP_EN); /* disable TMR1 compare */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyCtrl3Reg);
    
    MCR20Drv_DirectAccessSPIMultiByteWrite( (uint8_t) T1CMP_LSB, (uint8_t *) pWaitTimeout, 3);
    
    irqSts3Reg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
    irqSts3Reg &= ~(cIRQSTS3_TMR1MSK);      /* unmask TMR1 interrupt */
    irqSts3Reg &= 0xF0;                     /* do not change other IRQs status */
    irqSts3Reg |= (cIRQSTS3_TMR1IRQ);       /* aknowledge TMR1 IRQ */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, irqSts3Reg);
    
    phyCtrl3Reg |= cPHY_CTRL3_TMR1CMP_EN;   /* enable TMR1 compare */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyCtrl3Reg);
    
    OSA_InterruptEnable();
}
#endif

/*! *********************************************************************************
* \brief  Disable the TMR1 timeout
*
********************************************************************************** */
void PhyTimeDisableWaitTimeout
(
void
)
{
    uint8_t phyReg;

    OSA_InterruptDisable();

#if !gPhyUseReducedSpiAccess_d
    phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL3);
    phyReg &= ~(cPHY_CTRL3_TMR1CMP_EN); /* disable TMR1 compare */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyReg);
#endif

    phyReg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
    phyReg &= 0xF0;                     /* do not change IRQ status */
    phyReg |= cIRQSTS3_TMR1IRQ;         /* aknowledge TMR1 IRQ */
    phyReg |= cIRQSTS3_TMR1MSK;         /* mask TMR1 IRQ */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, phyReg);

    OSA_InterruptEnable();
}

/*! *********************************************************************************
* \brief  Set TMR4 timeout value
*
* \param[in]  pWakeUpTime  absolute time
*
********************************************************************************** */
#if gPhyUseReducedSpiAccess_d
void PhyTimeSetWakeUpTime(uint32_t *pWakeUpTime)
{
    uint8_t irqSts3Reg;
    
    OSA_InterruptDisable();
    
    irqSts3Reg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
    irqSts3Reg &= 0xF0;                  /* do not change other IRQs status */
    irqSts3Reg |= cIRQSTS3_TMR4MSK;      /* mask TMR4 interrupt */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, irqSts3Reg);
    
    MCR20Drv_DirectAccessSPIMultiByteWrite( (uint8_t) T4CMP_LSB, (uint8_t *) pWakeUpTime, 3);
    
    irqSts3Reg &= ~(cIRQSTS3_TMR4MSK);      /* unmask TMR4 interrupt */
    irqSts3Reg |= (cIRQSTS3_TMR4IRQ);       /* aknowledge TMR4 IRQ */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, irqSts3Reg);
    
    OSA_InterruptEnable();
}
#else
void PhyTimeSetWakeUpTime
(
uint32_t *pWakeUpTime
)
{
    uint8_t phyCtrl3Reg, irqSts3Reg;
    
    OSA_InterruptDisable();
    
    phyCtrl3Reg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL3);
    phyCtrl3Reg &= ~(cPHY_CTRL3_TMR4CMP_EN); /* disable TMR4 compare */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyCtrl3Reg);
    
    MCR20Drv_DirectAccessSPIMultiByteWrite( (uint8_t) T4CMP_LSB, (uint8_t *) pWakeUpTime, 3);
    
    irqSts3Reg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
    irqSts3Reg &= ~(cIRQSTS3_TMR4MSK);      /* unmask TMR4 interrupt */
    irqSts3Reg &= 0xF0;                     /* do not change other IRQs status */
    irqSts3Reg |= (cIRQSTS3_TMR4IRQ);       /* aknowledge TMR4 IRQ */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, irqSts3Reg);
    
    phyCtrl3Reg |= cPHY_CTRL3_TMR4CMP_EN;   /* enable TMR4 compare */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyCtrl3Reg);
    
    OSA_InterruptEnable();
}
#endif

/*! *********************************************************************************
* \brief  Check if TMR4 IRQ occured, and aknowledge it
*
* \return  TRUE if TMR4 IRQ occured
*
********************************************************************************** */
bool_t PhyTimeIsWakeUpTimeExpired
(
void
)
{
    bool_t wakeUpIrq = FALSE;
    uint8_t phyReg;
    
    OSA_InterruptDisable();
    
#if !gPhyUseReducedSpiAccess_d
    phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL3);
    phyReg &= ~(cPHY_CTRL3_TMR4CMP_EN); /* disable TMR4 compare */
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyReg);
#endif
    
    phyReg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
    
    if( (phyReg & cIRQSTS3_TMR4IRQ) == cIRQSTS3_TMR4IRQ )
    {
        wakeUpIrq = TRUE;
    }
    
    phyReg &= ~(cIRQSTS3_TMR4MSK);      /* unmask TMR4 interrupt */
    phyReg &= 0xF0;                     /* do not change other IRQs status */
    phyReg |= (cIRQSTS3_TMR4IRQ);       /* aknowledge TMR2 IRQ */
    
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, phyReg);
    
    OSA_InterruptEnable();
    
    return wakeUpIrq;
}


/*! *********************************************************************************
* \brief  PHY Timer Interrupt Service Routine
*
********************************************************************************** */
void PhyTime_ISR(void)
{
    if( pNextEvent->callback == PhyTime_OverflowCB )
    {
        gPhyTimerOverflow += (uint64_t)(1 << gPhyTimeShift_c);
    }
    
    if( gpfPhyTimeNotify )
    {
        gpfPhyTimeNotify();
    }
    else
    {
        PhyTime_RunCallback();
        PhyTime_Maintenance();
    }
}

/*! *********************************************************************************
* \brief  Initialize the PHY Timer module
*
* \return  phyTimeStatus_t
*
********************************************************************************** */
phyTimeStatus_t PhyTime_TimerInit( void (*cb)(void) )
{
    phyTimeStatus_t status = gPhyTimeOk_c;
    if( gpfPhyTimeNotify )
    {
        status = gPhyTimeError_c;
    }
    else
    {
        gpfPhyTimeNotify = cb;
        gPhyTimerOverflow = 0;
        FLib_MemSet( mPhyTimers, 0, sizeof(mPhyTimers) );
        
        /* Schedule Overflow Calback */
        pNextEvent = &mPhyTimers[0];
        pNextEvent->callback = PhyTime_OverflowCB;
        pNextEvent->timestamp = (uint64_t)(1 << gPhyTimeShift_c);
        PhyTimeSetWaitTimeout( &pNextEvent->timestamp );
        mPhyActiveTimers = 1;
    }

    return status;
}

/*! *********************************************************************************
* \brief  Returns a 64bit timestamp value to be used by the MAC Layer
*
* \return  phyTime_t PHY timestamp
*
********************************************************************************** */
phyTime_t PhyTime_GetTimestamp(void)
{
    phyTime_t t = 0;

    OSA_InterruptDisable();
    PhyTimeReadClock( &t );
    t |= gPhyTimerOverflow;
#if 0
    /* Check for overflow */
    if( pNextEvent->callback == PhyTime_OverflowCB )
    {
        if( MCR20Drv_DirectAccessSPIRead(IRQSTS3) & cIRQSTS3_TMR1IRQ )
        {
            t += (1 << gPhyTimeShift_c);
        }
    }
#endif
    OSA_InterruptEnable();

    return t;
}

/*! *********************************************************************************
* \brief  Schedules an event
*
* \param[in]  pEvent  event to be scheduled
*
* \return  phyTimeTimerId_t  the id of the alocated timer
*
********************************************************************************** */
phyTimeTimerId_t PhyTime_ScheduleEvent( phyTimeEvent_t *pEvent )
{
    phyTimeTimerId_t tmr;

    /* Parameter validation */
    if( NULL == pEvent->callback )
    {
        tmr = gInvalidTimerId_c;
    }
    else
    {
        /* Search for a free slot (slot 0 is reserved for the Overflow calback) */
        OSA_InterruptDisable();
        for( tmr=1; tmr<gMaxPhyTimers_c; tmr++ )
        {
            if( mPhyTimers[tmr].callback == NULL )
            {
                if( mPhyActiveTimers == 1 )
                {
                    PWR_DisallowXcvrToSleep();
                }

                mPhyActiveTimers++;
                mPhyTimers[tmr] = *pEvent;
                break;
            }
        }
        OSA_InterruptEnable();
        
        if( tmr >= gMaxPhyTimers_c )
        {
            tmr = gInvalidTimerId_c;
        }
        else
        {
            /* Program the next event */
            if((NULL == pNextEvent) ||
               ((NULL != pNextEvent)  && (mPhyTimers[tmr].timestamp < pNextEvent->timestamp)))
            {
                PhyTime_Maintenance();
            }
        }
    }
    return tmr;
}

/*! *********************************************************************************
* \brief  Cancel an event
*
* \param[in]  timerId  the Id of the timer
*
* \return  phyTimeStatus_t
*
********************************************************************************** */
phyTimeStatus_t PhyTime_CancelEvent( phyTimeTimerId_t timerId )
{
    phyTimeStatus_t status = gPhyTimeOk_c;

    if( (timerId == 0) || (timerId >= gMaxPhyTimers_c) || (NULL == mPhyTimers[timerId].callback) )
    {
        status = gPhyTimeNotFound_c;
    }
    else
    {
        OSA_InterruptDisable();
        if( pNextEvent == &mPhyTimers[timerId] )
        {
            pNextEvent = NULL;
        }
        
        mPhyTimers[timerId].callback = NULL;
        mPhyActiveTimers--;

        if( mPhyActiveTimers == 1 )
        {
            PWR_AllowXcvrToSleep();
        }

        OSA_InterruptEnable();
    }

    return status;
}

/*! *********************************************************************************
* \brief  Cancel all event with the specified paameter
*
* \param[in]  param  event parameter
*
* \return  phyTimeStatus_t
*
********************************************************************************** */
phyTimeStatus_t PhyTime_CancelEventsWithParam ( uint32_t param )
{
    uint32_t i;
    phyTimeStatus_t status = gPhyTimeNotFound_c;

    OSA_InterruptDisable();
    for( i=1; i<gMaxPhyTimers_c; i++ )
    {
        if( (NULL != mPhyTimers[i].callback) && (param == mPhyTimers[i].parameter) )
        {
            status = gPhyTimeOk_c;
            mPhyTimers[i].callback = NULL;
            mPhyActiveTimers--;

            if( pNextEvent == &mPhyTimers[i] )
            {
                pNextEvent = NULL;
            }
        }
    }

    if( mPhyActiveTimers == 1 )
    {
        PWR_AllowXcvrToSleep();
    }
    OSA_InterruptEnable();

    return status;
}

/*! *********************************************************************************
* \brief  Run the callback for the recently expired event
*
********************************************************************************** */
void PhyTime_RunCallback( void )
{
    uint32_t param;
    phyTimeCallback_t cb;

    if( pNextEvent )
    {
        OSA_InterruptDisable();

        param = pNextEvent->parameter;
        cb = pNextEvent->callback;
        pNextEvent->callback = NULL;
        pNextEvent = NULL;
        mPhyActiveTimers--;

        if( mPhyActiveTimers == 1 )
        {
            PWR_AllowXcvrToSleep();
        }

        OSA_InterruptEnable();

        cb(param);
    }
}

/*! *********************************************************************************
* \brief  Expire events too close to be scheduled.
*         Program the next event
*
********************************************************************************** */
void PhyTime_Maintenance( void )
{
    phyTime_t currentTime;
    phyTimeEvent_t *pEv;
    uint8_t irqSts3Reg;
#if !gPhyUseReducedSpiAccess_d
	uint8_t phyCtrl3Reg;
#endif

    ProtectFromMCR20Interrupt();

    /* Mask TMR1 IRQ */
    irqSts3Reg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
    irqSts3Reg &= 0xF0;                     /* do not change IRQ status */
    irqSts3Reg |= cIRQSTS3_TMR1MSK;
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS3, irqSts3Reg);
#if !gPhyUseReducedSpiAccess_d
    /* Disable TMR1 comparator */
    phyCtrl3Reg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL3);
    phyCtrl3Reg &= ~(cPHY_CTRL3_TMR1CMP_EN);
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL3, phyCtrl3Reg);
#endif
    UnprotectFromMCR20Interrupt();

    while(1)
    {
        ProtectFromMCR20Interrupt();

        pEv = PhyTime_GetNextEvent();

        /* Program next event if exists */
        if( pEv )
        {
            pNextEvent = pEv;

            /* write compare value */
            MCR20Drv_DirectAccessSPIMultiByteWrite( (uint8_t) T1CMP_LSB, (uint8_t *) &pEv->timestamp, 3);
#if !gPhyUseReducedSpiAccess_d
            /* Enable TMR1 comparator */
            phyCtrl3Reg |= cPHY_CTRL3_TMR1CMP_EN;   /* enable TMR1 compare */
#endif
            /* Enable TMR1 IRQ and clear status */                
            irqSts3Reg &= ~(cIRQSTS3_TMR1MSK);      /* unmask TMR1 interrupt */
            irqSts3Reg |= (cIRQSTS3_TMR1IRQ);       /* aknowledge TMR1 IRQ */

            OSA_InterruptDisable();
            
            currentTime = PhyTime_GetTimestamp();

            if( pEv->timestamp > (currentTime + gPhyTimeMinSetupTime_c) )
            {
#if !gPhyUseReducedSpiAccess_d
                MCR20Drv_DirectAccessSPIWrite( PHY_CTRL3, phyCtrl3Reg);
#endif
                MCR20Drv_DirectAccessSPIWrite( IRQSTS3, irqSts3Reg );
                pEv = NULL;
            }

            OSA_InterruptEnable();
        }

        UnprotectFromMCR20Interrupt();

        if( !pEv )
        {
            break;
        }

        PhyTime_RunCallback();
    }
}


/*! *********************************************************************************
* \brief  Timer Overflow callback
*
* \param[in]  param
*
********************************************************************************** */
static void PhyTime_OverflowCB( uint32_t param )
{
    param = param;

    /* Reprogram the next overflow callback */
    OSA_InterruptDisable();
    mPhyActiveTimers++;
    mPhyTimers[0].callback = PhyTime_OverflowCB;
    mPhyTimers[0].timestamp = gPhyTimerOverflow + (1 << gPhyTimeShift_c);
    OSA_InterruptEnable();
}

/*! *********************************************************************************
* \brief  Search for the next event to be scheduled
*
* \return phyTimeEvent_t pointer to the next event to be scheduled
*
********************************************************************************** */
static phyTimeEvent_t* PhyTime_GetNextEvent( void )
{
    phyTimeEvent_t *pEv = NULL;
    uint32_t i;

    /* Search for the next event to be serviced */
    for( i=0; i<gMaxPhyTimers_c; i++ )
    {
        if( NULL != mPhyTimers[i].callback )
        {
            if( NULL == pEv )
            {
                pEv = &mPhyTimers[i];
            }
            /* Check which event expires first */
            else
            {
                if( mPhyTimers[i].timestamp < pEv->timestamp )
                {
                    pEv = &mPhyTimers[i];
                }
            }
        }
    }

    return pEv;
}
