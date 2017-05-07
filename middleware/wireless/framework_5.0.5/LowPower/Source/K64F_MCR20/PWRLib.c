/*!
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* All rights reserved.
*
* \file PWRLib.c
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
#include "TimersManager.h"
#include "Keyboard.h"
#include "MCR20Drv.h"
#include "MCR20Reg.h"
#include "TMR_Adapter.h"
#include "GPIO_Adapter.h"
#include "gpio_pins.h"

#include "fsl_os_abstraction.h"
#include "fsl_lptmr.h"
#include "fsl_rtc.h"
#include "fsl_llwu.h"
#include "fsl_smc.h"


#define mLptmrTimoutMaxMs_c  65535000 

/*****************************************************************************
 *                               PRIVATE VARIABLES                           *
 *---------------------------------------------------------------------------*
 * Add to this section all the variables and constants that have local       *
 * (file) scope.                                                             *
 * Each of this declarations shall be preceded by the 'static' keyword.      *
 * These variables / constants cannot be accessed outside this module.       *
 *---------------------------------------------------------------------------*
 *****************************************************************************/

/* LPTMR/RTC variables */
   
#if (cPWR_UsePowerDownMode==1)
static uint32_t mPWRLib_RTIElapsedTicks;
#endif /* #if (cPWR_UsePowerDownMode==1) */


/* For LVD function */ 

#if (cPWR_LVD_Enable == 2)
tmrTimerID_t               PWRLib_LVD_PollIntervalTmrID;
PWRLib_LVD_VoltageLevel_t  PWRLib_LVD_SavedLevel;
#endif  /* #if (cPWR_LVD_Enable == 2) */


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

/* Zigbee STACK status */ 
PWRLib_StackPS_t PWRLib_StackPS;
volatile PWRLib_WakeupReason_t PWRLib_MCU_WakeupReason;

#if (cPWR_UsePowerDownMode==1)

/*****************************************************************************
 *                           PRIVATE FUNCTIONS PROTOTYPES                    *
 *---------------------------------------------------------------------------*
 * Add to this section all the functions prototypes that have local (file)   *
 * scope.                                                                    *
 * These functions cannot be accessed outside this module.                   *
 * These declarations shall be preceded by the 'static' keyword.             *
 *---------------------------------------------------------------------------*
 *****************************************************************************/
static void PWRLib_Radio_Force_Idle(void);


/*****************************************************************************
 *                                PRIVATE FUNCTIONS                          *
 *---------------------------------------------------------------------------*
 * Add to this section all the functions that have local (file) scope.       *
 * These functions cannot be accessed outside this module.                   *
 * These definitions shall be preceded by the 'static' keyword.              *
 *---------------------------------------------------------------------------*
*****************************************************************************/
static void PWRLib_Radio_Force_Idle(void)
{
    uint8_t irqSts1Reg;
    uint8_t phyCtrl1Reg = MCR20Drv_DirectAccessSPIRead( (uint8_t) PHY_CTRL1 );

    if( (phyCtrl1Reg & cPHY_CTRL1_XCVSEQ) != 0x00 )
    {
        /* abort any ongoing sequence */
        /* make sure that we abort in HW only if the sequence was actually started (tmr triggered) */
        if( ( 0 != ( MCR20Drv_DirectAccessSPIRead( (uint8_t) PHY_CTRL1) & cPHY_CTRL1_XCVSEQ ) ) && ((MCR20Drv_DirectAccessSPIRead(SEQ_STATE)&0x1F) != 0))
        {
            phyCtrl1Reg &= (uint8_t) ~(cPHY_CTRL1_XCVSEQ);
            MCR20Drv_DirectAccessSPIWrite(PHY_CTRL1, phyCtrl1Reg);
            while ((MCR20Drv_DirectAccessSPIRead(SEQ_STATE) & 0x1F) != 0);
        }
        /* clear sequence-end interrupt */ 
        irqSts1Reg = MCR20Drv_DirectAccessSPIRead( (uint8_t) IRQSTS1);
        irqSts1Reg |= (uint8_t) cIRQSTS1_SEQIRQ;
        MCR20Drv_DirectAccessSPIWrite( (uint8_t) IRQSTS1, irqSts1Reg);
    }
}


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
 * Name: PWRLib_Radio_Enter_Doze
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_Radio_Enter_Doze
(
void
)
{
  uint8_t pwrModesReg;
  OSA_DisableIRQGlobal();
  pwrModesReg = MCR20Drv_DirectAccessSPIRead( (uint8_t) PWR_MODES);
  /* disable autodoze mode. sets PMC in low-power mode */
  pwrModesReg &= (uint8_t) ~( cPWR_MODES_AUTODOZE | cPWR_MODES_PMC_MODE );
  /* check if 32 MHz crystal oscillator is enabled (current state is hibernate mode) */
  if( (pwrModesReg & cPWR_MODES_XTALEN ) != cPWR_MODES_XTALEN )
  {
    /* enable 32 MHz crystal oscillator */
    pwrModesReg |= (uint8_t) cPWR_MODES_XTALEN;
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PWR_MODES, pwrModesReg);
    /* wait for crystal oscillator to complet its warmup */
    while( ( MCR20Drv_DirectAccessSPIRead( (uint8_t) PWR_MODES) & cPWR_MODES_XTAL_READY ) != cPWR_MODES_XTAL_READY);
    /* wait for radio wakeup from hibernate interrupt */
    while( ( MCR20Drv_DirectAccessSPIRead( (uint8_t) IRQSTS2) & (cIRQSTS2_WAKE_IRQ | cIRQSTS2_TMRSTATUS) ) != (cIRQSTS2_WAKE_IRQ | cIRQSTS2_TMRSTATUS) );
    MCR20Drv_DirectAccessSPIWrite((uint8_t) IRQSTS2, (uint8_t) (cIRQSTS2_WAKE_IRQ));
  }
  else
  {
    /* checks if packet processor is in idle state. otherwise abort any ongoing sequence */
    PWRLib_Radio_Force_Idle();
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PWR_MODES, pwrModesReg);
  }
  OSA_EnableIRQGlobal();
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_Radio_Enter_AutoDoze
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_Radio_Enter_AutoDoze
(
void
)
{
  uint8_t pwrModesReg;
  OSA_DisableIRQGlobal();
  pwrModesReg = MCR20Drv_DirectAccessSPIRead( (uint8_t) PWR_MODES);
  /* enable autodoze mode. */
  pwrModesReg |= (uint8_t) cPWR_MODES_AUTODOZE;
  /* check if 32 MHz crystal oscillator is enabled (current state is hibernate mode) */
  if( (pwrModesReg & cPWR_MODES_XTALEN ) != cPWR_MODES_XTALEN )
  {
    /* enable 32 MHz crystal oscillator */
    pwrModesReg |= (uint8_t) cPWR_MODES_XTALEN;
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PWR_MODES, pwrModesReg);
    /* wait for crystal oscillator to complet its warmup */
    while( ( MCR20Drv_DirectAccessSPIRead( (uint8_t) PWR_MODES) & cPWR_MODES_XTAL_READY ) != cPWR_MODES_XTAL_READY);
    /* wait for radio wakeup from hibernate interrupt */
    while( ( MCR20Drv_DirectAccessSPIRead( (uint8_t) IRQSTS2) & (cIRQSTS2_WAKE_IRQ | cIRQSTS2_TMRSTATUS) ) != (cIRQSTS2_WAKE_IRQ | cIRQSTS2_TMRSTATUS) );
    MCR20Drv_DirectAccessSPIWrite((uint8_t) IRQSTS2, (uint8_t) (cIRQSTS2_WAKE_IRQ));
  }
  else
  {
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PWR_MODES, pwrModesReg);
  }
  OSA_EnableIRQGlobal();
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_Radio_Enter_Idle
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_Radio_Enter_Idle
(
void
)
{
  uint8_t pwrModesReg;
  OSA_DisableIRQGlobal();
  pwrModesReg = MCR20Drv_DirectAccessSPIRead( (uint8_t) PWR_MODES);
  /* disable autodoze mode. sets PMC in high-power mode */
  pwrModesReg &= (uint8_t) ~( cPWR_MODES_AUTODOZE );
  pwrModesReg |= (uint8_t) cPWR_MODES_PMC_MODE;
  /* check if 32 MHz crystal oscillator is enabled (current state is hibernate mode) */
  if( (pwrModesReg & cPWR_MODES_XTALEN ) != cPWR_MODES_XTALEN )
  {
    /* enable 32 MHz crystal oscillator */
    pwrModesReg |= (uint8_t) cPWR_MODES_XTALEN;
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PWR_MODES, pwrModesReg);
    /* wait for crystal oscillator to complet its warmup */
    while( ( MCR20Drv_DirectAccessSPIRead( (uint8_t) PWR_MODES) & cPWR_MODES_XTAL_READY ) != cPWR_MODES_XTAL_READY);
    /* wait for radio wakeup from hibernate interrupt */
    while( ( MCR20Drv_DirectAccessSPIRead( (uint8_t) IRQSTS2) & (cIRQSTS2_WAKE_IRQ | cIRQSTS2_TMRSTATUS) ) != (cIRQSTS2_WAKE_IRQ | cIRQSTS2_TMRSTATUS) );
    MCR20Drv_DirectAccessSPIWrite((uint8_t) IRQSTS2, (uint8_t) (cIRQSTS2_WAKE_IRQ));
  }
  else
  {
    /* checks if packet processor is in idle state. otherwise abort any ongoing sequence */
    PWRLib_Radio_Force_Idle();
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PWR_MODES, pwrModesReg);
  }
  OSA_EnableIRQGlobal();
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_Radio_Enter_Hibernate
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_Radio_Enter_Hibernate
(
void
)
{
  uint8_t pwrModesReg;
  OSA_DisableIRQGlobal();
  /* checks if packet processor is in idle state. otherwise abort any ongoing sequence */
  PWRLib_Radio_Force_Idle();
  
  pwrModesReg = MCR20Drv_DirectAccessSPIRead( (uint8_t) PWR_MODES);
  /* disable autodoze mode. disable 32 MHz crystal oscillator. sets PMC in low-power mode */
  pwrModesReg &= (uint8_t) ~( cPWR_MODES_AUTODOZE | cPWR_MODES_XTALEN | cPWR_MODES_PMC_MODE );
  
  MCR20Drv_DirectAccessSPIWrite( (uint8_t) PWR_MODES, pwrModesReg);
  
  //  {
  //    uint8_t tmpReg;
  //    tmpReg = MCR20Drv_DirectAccessSPIRead( (uint8_t) PWR_MODES);
  //    while( cPWR_MODES_XTAL_READY == ( tmpReg & cPWR_MODES_XTAL_READY ) )
  //    {
  //      MCR20Drv_DirectAccessSPIWrite( (uint8_t) PWR_MODES, pwrModesReg);
  //      tmpReg = MCR20Drv_DirectAccessSPIRead( (uint8_t) PWR_MODES);
  //    }
  //  }
  OSA_EnableIRQGlobal();
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_MCUEnter_Sleep
 * Description: Puts the processor into Sleep .

                Mode of operation details:
                 - ARM core enters Sleep Mode
                 - ARM core is clock gated (HCLK = OFF)
                 - peripherals are functional
                
                

 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_MCU_Enter_Sleep
(
void
)
{
  /* SCB_SCR: SLEEPDEEP=0 */
  SCB->SCR &= (uint32_t)~(uint32_t)(SCB_SCR_SLEEPDEEP_Msk);
  
  /* SCB_SCR: SLEEPONEXIT=0 */
  SCB->SCR &= (uint32_t)~(uint32_t)(SCB_SCR_SLEEPONEXIT_Msk);
  /* WFI instruction will start entry into deep sleep mode */
  asm("WFI");
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_MCUEnter_VLLS0
 * Description: Puts the processor into VLLS0 (Very Low Leakage Stop1).

                Mode of operation details:
                 - ARM core enters SleepDeep Mode
                 - ARM core is clock gated (HCLK = OFF)
                 - NVIC is disable (FCLK = OFF)
                 - LLWU should configure by user to enable the desire wake up source
                 - Platform and peripheral clock are stopped
                 - All SRAM powered off
                 - VLLS0 mode is exited into RUN mode using LLWU module or RESET.
                   All wakeup goes through Reset sequence.

                The AVLLS must be set to 0b1 in SMC_PMPROT register in order to allow VLLS mode.

 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_MCU_Enter_VLLS0
(
void
)
{
  LLWU->F1 = LLWU->F1;
  LLWU->F2 = LLWU->F2;
  LLWU->FILT1 |= LLWU_FILT1_FILTF_MASK;
  LLWU->FILT2 |= LLWU_FILT2_FILTF_MASK;
  SCB->SCR &= (uint32_t)~(uint32_t)(SCB_SCR_SLEEPONEXIT_Msk);
  /* Set the SLEEPDEEP bit to enable CORTEX M4 deep sleep mode */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  SMC->PMCTRL = (uint8_t)((SMC->PMCTRL & (uint8_t)~(uint8_t)(SMC_PMCTRL_STOPM(0x03))) | (uint8_t)(SMC_PMCTRL_STOPM(0x04)));
#if cPWR_POR_DisabledInVLLS0
  SMC->VLLSCTRL = SMC_VLLSCTRL_PORPO_MASK | SMC_VLLSCTRL_VLLSM(0x00);
#else
  SMC->VLLSCTRL = SMC_VLLSCTRL_VLLSM(0x00);
#endif  
  (void)(SMC->PMCTRL == 0U);        /* Dummy read of SMC_PMCTRL to ensure the register is written before enterring low power mode */
   /* WFI instruction will start entry into deep sleep mode */
  asm("WFI");
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_MCUEnter_VLLS1
 * Description: Puts the processor into VLLS1 (Very Low Leakage Stop1).

                Mode of operation details:
                 - ARM core enters SleepDeep Mode
                 - ARM core is clock gated (HCLK = OFF)
                 - NVIC is disable (FCLK = OFF)
                 - LLWU should configure by user to enable the desire wake up source
                 - Platform and peripheral clock are stopped
                 - All SRAM powered off
                 - VLLS1 mode is exited into RUN mode using LLWU module or RESET.
                All wakeup goes through Reset sequence.

                The AVLLS must be set to 0b1 in MC_PMPROT register in order to allow VLLS mode.

 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_MCU_Enter_VLLS1
(
void
)
{
  LLWU->F1 = LLWU->F1;
  LLWU->F2 = LLWU->F2;
  LLWU->FILT1 |= LLWU_FILT1_FILTF_MASK;
  LLWU->FILT2 |= LLWU_FILT2_FILTF_MASK;
  SCB->SCR &= (uint32_t)~(uint32_t)(SCB_SCR_SLEEPONEXIT_Msk);
  /* Set the SLEEPDEEP bit to enable CORTEX M4 deep sleep mode */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  SMC->PMCTRL = (uint8_t)((SMC->PMCTRL & (uint8_t)~(uint8_t)(SMC_PMCTRL_STOPM(0x03))) | (uint8_t)(SMC_PMCTRL_STOPM(0x04)));
  SMC->VLLSCTRL = SMC_VLLSCTRL_VLLSM(0x01);
  (void)(SMC->PMCTRL == 0U);        /* Dummy read of SMC_PMCTRL to ensure the register is written before enterring low power mode */
   /* WFI instruction will start entry into deep sleep mode */
  asm("WFI");
}
/*---------------------------------------------------------------------------
 * Name: PWRLib_MCUEnter_VLLS2
 * Description: Puts the processor into VLLS1 (Very Low Leakage Stop1).

                Mode of operation details:
                 - ARM core enters SleepDeep Mode
                 - ARM core is clock gated (HCLK = OFF)
                 - NVIC is disable (FCLK = OFF)
                 - LLWU should configure by user to enable the desire wake up source
                 - Platform and peripheral clock are stopped
                 - All SRAM powered off
                 - VLLS1 mode is exited into RUN mode using LLWU module or RESET.
                All wakeup goes through Reset sequence.

                The AVLLS must be set to 0b1 in MC_PMPROT register in order to allow VLLS mode.

 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_MCU_Enter_VLLS2
(
void
)
{
  LLWU->F1 = LLWU->F1;
  LLWU->F2 = LLWU->F2;
  LLWU->FILT1 |= LLWU_FILT1_FILTF_MASK;
  LLWU->FILT2 |= LLWU_FILT2_FILTF_MASK;
  SCB->SCR &= (uint32_t)~(uint32_t)(SCB_SCR_SLEEPONEXIT_Msk);
  /* Set the SLEEPDEEP bit to enable CORTEX M4 deep sleep mode */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  SMC->PMCTRL = (uint8_t)((SMC->PMCTRL & (uint8_t)~(uint8_t)(SMC_PMCTRL_STOPM(0x03))) | (uint8_t)(SMC_PMCTRL_STOPM(0x04)));
  SMC->VLLSCTRL = SMC_VLLSCTRL_VLLSM(0x02);
  (void)(SMC->PMCTRL == 0U);        /* Dummy read of SMC_PMCTRL to ensure the register is written before enterring low power mode */
   /* WFI instruction will start entry into deep sleep mode */
  asm("WFI");
}
/*---------------------------------------------------------------------------
 * Name: PWRLib_MCUEnter_LLS3
 * Description: Puts the processor into LLS3 (Low Leakage Stop1).

                Mode of operation details:
                 - ARM core enters SleepDeep Mode
                 - ARM core is clock gated (HCLK = OFF)
                 - NVIC is disable (FCLK = OFF)
                 - LLWU should configure by user to enable the desire wake up source
                 - Platform and peripheral clock are stopped
                 - Full SRAM retention.               

                The ALLS must be set to 0b1 in SMC_PMPROT register in order to allow LLS mode.

 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_MCU_Enter_LLS3
(
void
)
{
  LLWU->F1 = LLWU->F1;
  LLWU->F2 = LLWU->F2;
  LLWU->FILT1 |= LLWU_FILT1_FILTF_MASK;
  LLWU->FILT2 |= LLWU_FILT2_FILTF_MASK;
  SCB->SCR &= (uint32_t)~(uint32_t)(SCB_SCR_SLEEPONEXIT_Msk);
  /* Set the SLEEPDEEP bit to enable CORTEX M0 deep sleep mode */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  SMC->PMCTRL = (uint8_t)((SMC->PMCTRL & (uint8_t)~(uint8_t)(SMC_PMCTRL_STOPM(0x04))) | (uint8_t)(SMC_PMCTRL_STOPM(0x03)));
  SMC->VLLSCTRL = SMC_VLLSCTRL_VLLSM(0x03);
  (void)(SMC->PMCTRL == 0U);        /* Dummy read of SMC_PMCTRL to ensure the register is written before enterring low power mode */
   /* WFI instruction will start entry into deep sleep mode */
  asm("WFI");
}
/*---------------------------------------------------------------------------
 * Name: PWRLib_MCUEnter_Stop
 * Description: Puts the processor into Stop 

                Mode of operation details:
                 - ARM core enters SleepDeep Mode
                 - ARM core is clock gated (HCLK = OFF)
                 - NVIC is disable (FCLK = OFF)
                 - Platform and peripheral clock are stopped
                 - Full SRAM retention.               
                The ALLS must be set to 0b1 in SMC_PMPROT register in order to allow LLS mode.

 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_MCU_Enter_Stop
(
void
)
{
  SCB->SCR &= (uint32_t)~(uint32_t)(SCB_SCR_SLEEPONEXIT_Msk);
  /* Set the SLEEPDEEP bit to enable CORTEX M4 deep sleep mode */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  SMC->PMCTRL = (uint8_t)(SMC->PMCTRL & (uint8_t)~(uint8_t)(SMC_PMCTRL_STOPM(0x07)));
  SMC->VLLSCTRL = SMC_VLLSCTRL_VLLSM(0x00);
  (void)(SMC->PMCTRL == 0U);        /* Dummy read of SMC_PMCTRL to ensure the register is written before enterring low power mode */
   /* WFI instruction will start entry into deep sleep mode */
  asm("WFI");
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_MCUEnter_VLPS
 * Description: Puts the processor into Verry-Low_Power-Stop 

                Mode of operation details:
                 - ARM core enters SleepDeep Mode
                 - ARM core is clock gated (HCLK = OFF)
                 - NVIC is disable (FCLK = OFF)
                 - Platform and peripheral clock are stopped
                 - Full SRAM retention.               
                The ALLS must be set to 0b1 in SMC_PMPROT register in order to allow LLS mode.

 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_MCU_Enter_VLPS
(
void
)
{
  SCB->SCR &= (uint32_t)~(uint32_t)(SCB_SCR_SLEEPONEXIT_Msk);
  /* Set the SLEEPDEEP bit to enable CORTEX M4 deep sleep mode */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  SMC->PMCTRL = (uint8_t)((SMC->PMCTRL & (uint8_t)~(uint8_t)(SMC_PMCTRL_STOPM(0x07))) | (uint8_t)(SMC_PMCTRL_STOPM(0x02)));
  SMC->VLLSCTRL = SMC_VLLSCTRL_VLLSM(0x00);
  (void)(SMC->PMCTRL == 0U);        /* Dummy read of SMC_PMCTRL to ensure the register is written before enterring low power mode */
   /* WFI instruction will start entry into deep sleep mode */
  asm("WFI");
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_LLWU_GetWakeUpFlags
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
uint32_t PWRLib_LLWU_GetWakeUpFlags(void)
{
  uint32_t Flags;

  Flags = LLWU->F1;
  Flags |= (uint32_t)((uint32_t)LLWU->F2 << 8U);
  Flags |= (uint32_t)((uint32_t)LLWU->F3 << 16U);
  if ((LLWU->FILT1 & 0x80U) != 0x00U ) {
    Flags |= LLWU_FILTER1;
  }
  if ((LLWU->FILT2 & 0x80U) != 0x00U ) {
    Flags |= LLWU_FILTER2;
  }
  return Flags;
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_LLWU_UpdateWakeupReason
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_LLWU_UpdateWakeupReason
(
void
)
{
    uint32_t  llwuFlags =  PWRLib_LLWU_GetWakeUpFlags();

#if  BOARD_LLWU_PIN_ENABLE_BITMAP
    if(llwuFlags & BOARD_LLWU_PIN_ENABLE_BITMAP)
    {
        PWRLib_MCU_WakeupReason.Bits.FromKeyBoard = 1; 
    }
#endif  
    
    if( llwuFlags & gPWRLib_LLWU_WakeupModuleFlag_LPTMR_c )
    {
        PWRLib_MCU_WakeupReason.Bits.FromLPTMR = 1;
        PWRLib_MCU_WakeupReason.Bits.DeepSleepTimeout = 1;
    }
    
    if( llwuFlags &gPWRLib_LLWU_WakeupModuleFlag_RTC_Alarm_c )
    {
        PWRLib_MCU_WakeupReason.Bits.FromRTC = 1;
        PWRLib_MCU_WakeupReason.Bits.DeepSleepTimeout = 1;
    }
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_LLWU_DisableAllWakeupSources
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_LLWU_DisableAllWakeupSources()
{
   LLWU->PE1 = 0;
   LLWU->PE2 = 0;
   LLWU->PE3 = 0;
   LLWU->PE4 = 0;
   LLWU->ME = 0;
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_LPTMR_Isr
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_LPTMR_Isr(void)
{
  LPTMR0->CSR |= LPTMR_CSR_TCF_MASK;
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_LPTMR_Init
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_LPTMR_Init
(
void
)
{
  
  OSA_InstallIntHandler(LPTMR0_IRQn, PWRLib_LPTMR_Isr);
  SIM->SCGC5 |= SIM_SCGC5_LPTMR_MASK;
  
  LPTMR0->CSR = (LPTMR_CSR_TCF_MASK | LPTMR_CSR_TPS(0x00)); /* Clear control register */
  LPTMR0->CMR = LPTMR_CMR_COMPARE(0x02); /* Set up compare register */
  NVIC_SetPriority(LPTMR0_IRQn, 0x80);
  NVIC_EnableIRQ(LPTMR0_IRQn);
  LPTMR0->CSR = (LPTMR_CSR_TIE_MASK | LPTMR_CSR_TPS(0x00)); /* Set up control register */
 
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_LPTMR_DeInit
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_LPTMR_DeInit
(
void
)
{
  LPTMR0->CSR = LPTMR_CSR_TCF_MASK; /* Clear control register */
  SIM->SCGC5 &= ~SIM_SCGC5_LPTMR_MASK;
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_LPTMR_GetTimeSettings
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_LPTMR_GetTimeSettings(uint32_t timeInMs, uint8_t* pClkMode,uint32_t* pTicks, uint32_t* pFreq)
{
    uint32_t ticks;
    uint32_t freq;
    uint8_t bypass = LPTMR_PSR_PBYP_MASK;
    uint8_t prescaler = 0;
#if (cPWR_LPTMRClockSource == cLPTMR_Source_Ext_ERCLK32K)
    uint64_t ticks64;
    
    if(timeInMs > mLptmrTimoutMaxMs_c)
    {
        timeInMs = mLptmrTimoutMaxMs_c;
    }
    
    freq = 32768;
    ticks64 = timeInMs;
    ticks64 *= freq;
    ticks64 /= 1000;
    ticks = ticks64;  
    *pClkMode = cLPTMR_Source_Ext_ERCLK32K;
    
#elif (cPWR_LPTMRClockSource == cLPTMR_Source_Int_LPO_1KHz)

    if(timeInMs > mLptmrTimoutMaxMs_c)
    {
        timeInMs = mLptmrTimoutMaxMs_c;
    }
    
    freq = 1000;
    ticks = timeInMs;
    *pClkMode = cLPTMR_Source_Int_LPO_1KHz;
    
#else
#error Wrong LPTMR clock source!
#endif
    
    while(ticks & 0xFFFF0000)
    {
        if(bypass)
        {
            bypass = 0;
        }
        else
        {
            prescaler++;
        }
        ticks >>= 1;
        freq >>= 1;
    }
    
    *pClkMode |= (prescaler<<LPTMR_PSR_PRESCALE_SHIFT)| bypass;
    *pTicks = ticks;
    *pFreq = freq;
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_LPTMR_ClockStart
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_LPTMR_ClockStart
(
uint8_t  ClkMode,
uint32_t Ticks
)
{
    OSA_DisableIRQGlobal();
    LPTMR0->CSR &= ~LPTMR_CSR_TEN_MASK;
    /* Set compare value */
    if(Ticks)
    {
        Ticks--;
    }
    LPTMR0->CMR = Ticks;
    /* Use specified tick count */
    mPWRLib_RTIElapsedTicks = 0;
    /* Configure prescaler, bypass prescaler and clck source */
    LPTMR0->PSR = ClkMode;
    LPTMR0->CSR |= LPTMR_CSR_TFC_MASK;
    /* Start counting */
    LPTMR0->CSR |= LPTMR_CSR_TEN_MASK;
    OSA_EnableIRQGlobal();
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_LPTMR_ClockCheck
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
uint32_t PWRLib_LPTMR_ClockCheck
(
void
)
{
    OSA_DisableIRQGlobal();
    /* LPTMR is still running */
    if(LPTMR0->CSR & LPTMR_CSR_TEN_MASK)
    {
        LPTMR0->CNR = 0;// CNR must be written first in order to be read 
        mPWRLib_RTIElapsedTicks = LPTMR0->CNR;
        /* timer compare flag is set */
        if(LPTMR0->CSR & LPTMR_CSR_TCF_MASK)
        {
            uint32_t compareReg;
            compareReg = LPTMR0->CMR;
            if(mPWRLib_RTIElapsedTicks < compareReg )
            {
                mPWRLib_RTIElapsedTicks += 0x10000;
            }
        }
    }
    OSA_EnableIRQGlobal();
    return mPWRLib_RTIElapsedTicks;
}



/*---------------------------------------------------------------------------
 * Name: PWRLib_LPTMR_ClockStop
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_LPTMR_ClockStop
(
void
)
{
    OSA_DisableIRQGlobal();
    /* LPTMR is still running */
    if(LPTMR0->CSR & LPTMR_CSR_TEN_MASK)
    {
        LPTMR0->CNR = 0;// CNR must be written first in order to be read 
        mPWRLib_RTIElapsedTicks = LPTMR0->CNR;
        /* timer compare flag is set */
        if(LPTMR0->CSR & LPTMR_CSR_TCF_MASK)
        {
            uint32_t compareReg;
            compareReg = LPTMR0->CMR;
            if(mPWRLib_RTIElapsedTicks < compareReg )
            {
                mPWRLib_RTIElapsedTicks += 0x10000;
            }
        }
    }
    /* Stop LPTMR */
    LPTMR0->CSR &= ~LPTMR_CSR_TEN_MASK;
    OSA_EnableIRQGlobal();
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_LLWU_Isr
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/

void PWRLib_LLWU_Isr
(
void
)
{
  uint32_t  llwuFlags =  PWRLib_LLWU_GetWakeUpFlags();

  /* Clear external pins wakeup interrupts */
  LLWU->F1 = LLWU->F1; 
  LLWU->F2 = LLWU->F2; 
    
  /* LPTMR is wakeup source */
  if(llwuFlags & gPWRLib_LLWU_WakeupModuleFlag_LPTMR_c)  
  {
    /* Clear LPTMR interrupt */
    LPTMR0->CSR |= LPTMR_CSR_TCF_MASK;
  }

  /* RTC alarm is wakeup source */
  if(llwuFlags & gPWRLib_LLWU_WakeupModuleFlag_RTC_Alarm_c)
  {
      /* Clear RTC interrupt */
      RTC->TSR = RTC->TSR;
  }
}

#endif /* #if (cPWR_UsePowerDownMode==1) */




/*---------------------------------------------------------------------------
* Name: PWRLib_LVD_CollectLevel
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
PWRLib_LVD_VoltageLevel_t PWRLib_LVD_CollectLevel
(
void
)
{
#if ((cPWR_LVD_Enable == 1) || (cPWR_LVD_Enable == 2))
  
  /* Check low detect voltage 1.6V */
  PMC->LVDSC1 = PMC_LVDSC1_LVDV(0);
  PMC->LVDSC2 = PMC_LVDSC2_LVWV(0);
  PMC->LVDSC1 = PMC_LVDSC1_LVDACK_MASK;
  if(PMC->LVDSC1 & PMC_LVDSC1_LVDF_MASK)
  {
    /* Low detect voltage reached */
    PMC->LVDSC1 = PMC_LVDSC1_LVDACK_MASK;
    return(PWR_LEVEL_CRITICAL);
  }
  
  /* Check low trip voltage 1.8V */
  PMC->LVDSC1 = PMC_LVDSC1_LVDV(0);
  PMC->LVDSC2 = PMC_LVDSC2_LVWV(0);
  PMC->LVDSC2 |= PMC_LVDSC2_LVWACK_MASK;
  if(PMC->LVDSC2 & PMC_LVDSC2_LVWF_MASK)
  {
    /* Low trip voltage reached */
    PMC->LVDSC2 = PMC_LVDSC2_LVWACK_MASK; /* Clear flag (and set low trip voltage) */
    PMC->LVDSC1 = PMC_LVDSC1_LVDV(0); /* Set low trip voltage */
    return(PWR_BELOW_LEVEL_1_8V);
  }
  
  /* Check low trip voltage 1.9V */
  PMC->LVDSC1 = PMC_LVDSC1_LVDV(0);
  PMC->LVDSC2 = PMC_LVDSC2_LVWV(1);
  PMC->LVDSC2 |= PMC_LVDSC2_LVWACK_MASK;
  if(PMC->LVDSC2 & PMC_LVDSC2_LVWF_MASK)
  {
    /* Low trip voltage reached */
    PMC->LVDSC2 = PMC_LVDSC2_LVWACK_MASK; /* Clear flag (and set low trip voltage) */
    PMC->LVDSC1 = PMC_LVDSC1_LVDV(0); /* Set low trip voltage */
    return(PWR_BELOW_LEVEL_1_9V);
  }
  /* Check low trip voltage 2.0V */
  PMC->LVDSC1 = PMC_LVDSC1_LVDV(0);
  PMC->LVDSC2 = PMC_LVDSC2_LVWV(2);
  PMC->LVDSC2 |= PMC_LVDSC2_LVWACK_MASK;
  if(PMC->LVDSC2 & PMC_LVDSC2_LVWF_MASK)
  {
    /* Low trip voltage reached */
    PMC->LVDSC2 = PMC_LVDSC2_LVWACK_MASK; /* Clear flag (and set low trip voltage) */
    PMC->LVDSC1 = PMC_LVDSC1_LVDV(0); /* Set low trip voltage */
    return(PWR_BELOW_LEVEL_2_0V);
  }
  
  /* Check low trip voltage 2.1V */
  PMC->LVDSC1 = PMC_LVDSC1_LVDV(0);
  PMC->LVDSC2 = PMC_LVDSC2_LVWV(3);
  PMC->LVDSC2 |= PMC_LVDSC2_LVWACK_MASK;
  if(PMC->LVDSC2 & PMC_LVDSC2_LVWF_MASK)
  {
    /* Low trip voltage reached */
    PMC->LVDSC2 = PMC_LVDSC2_LVWACK_MASK; /* Clear flag (and set low trip voltage) */
    PMC->LVDSC1 = PMC_LVDSC1_LVDV(0); /* Set low trip voltage */
    return(PWR_BELOW_LEVEL_2_1V);
  }
  
  /* Check low detect voltage (high range) 2.56V */
  PMC->LVDSC1 = PMC_LVDSC1_LVDV(1); /* Set high trip voltage and clear warning flag */
  PMC->LVDSC2 = PMC_LVDSC2_LVWV(0);
  PMC->LVDSC1 |= PMC_LVDSC1_LVDACK_MASK;
  if(PMC->LVDSC1 & PMC_LVDSC1_LVDF_MASK)
  {
    /* Low detect voltage reached */
    PMC->LVDSC1 = PMC_LVDSC1_LVDACK_MASK; /* Set low trip voltage and clear warning flag */
    PMC->LVDSC1 = PMC_LVDSC1_LVDV(0); /* Set low trip voltage */
    return(PWR_BELOW_LEVEL_2_56V);
  }
  
  /* Check high trip voltage 2.7V */
  PMC->LVDSC1 = PMC_LVDSC1_LVDV(1);
  PMC->LVDSC2 = PMC_LVDSC2_LVWV(0);
  PMC->LVDSC2 |= PMC_LVDSC2_LVWACK_MASK;
  if(PMC->LVDSC2 & PMC_LVDSC2_LVWF_MASK)
  {
    /* Low trip voltage reached */
    PMC->LVDSC2 = PMC_LVDSC2_LVWACK_MASK; /* Clear flag (and set low trip voltage) */
    PMC->LVDSC1 = PMC_LVDSC1_LVDV(0); /* Set low trip voltage */
    return(PWR_BELOW_LEVEL_2_7V);
  }
  
  /* Check high trip voltage 2.8V */
  PMC->LVDSC1 = PMC_LVDSC1_LVDV(1);
  PMC->LVDSC2 = PMC_LVDSC2_LVWV(1);
  PMC->LVDSC2 |= PMC_LVDSC2_LVWACK_MASK;
  if(PMC->LVDSC2 & PMC_LVDSC2_LVWF_MASK)
  {
    /* Low trip voltage reached */
    PMC->LVDSC2 = PMC_LVDSC2_LVWACK_MASK; /* Clear flag (and set low trip voltage) */
    PMC->LVDSC1 = PMC_LVDSC1_LVDV(0); /* Set low trip voltage */
    return(PWR_BELOW_LEVEL_2_8V);
  }
  
  /* Check high trip voltage 2.9V */
  PMC->LVDSC1 = PMC_LVDSC1_LVDV(1);
  PMC->LVDSC2 = PMC_LVDSC2_LVWV(2);
  PMC->LVDSC2 |= PMC_LVDSC2_LVWACK_MASK;
  if(PMC->LVDSC2 & PMC_LVDSC2_LVWF_MASK)
  {
    /* Low trip voltage reached */
    PMC->LVDSC2 = PMC_LVDSC2_LVWACK_MASK; /* Clear flag (and set low trip voltage) */
    PMC->LVDSC1 = PMC_LVDSC1_LVDV(0); /* Set low trip voltage */
    return(PWR_BELOW_LEVEL_2_9V);
  }
  
  /* Check high trip voltage 3.0V */
  PMC->LVDSC1 = PMC_LVDSC1_LVDV(1);
  PMC->LVDSC2 = PMC_LVDSC2_LVWV(3);
  PMC->LVDSC2 |= PMC_LVDSC2_LVWACK_MASK;
  if(PMC->LVDSC2 & PMC_LVDSC2_LVWF_MASK)
  {
    /* Low trip voltage reached */
    PMC->LVDSC2 = PMC_LVDSC2_LVWACK_MASK; /* Clear flag (and set low trip voltage) */
    PMC->LVDSC1 = PMC_LVDSC1_LVDV(0); /* Set low trip voltage */
    return(PWR_BELOW_LEVEL_3_0V);
  }
  
  PMC->LVDSC2 = PMC_LVDSC2_LVWV(0);
  PMC->LVDSC1 = PMC_LVDSC1_LVDV(0); /* Set low trip voltage */
#endif  /* #if ((cPWR_LVD_Enable == 1) || (cPWR_LVD_Enable == 2)) */
  
  /*--- Voltage level is okay > 3.0V */
  return(PWR_ABOVE_LEVEL_3_0V);
}

/******************************************************************************
 * Name: PWRLib_LVD_PollIntervalCallback
 * Description:
 *
 * Parameter(s): -
 * Return: -
 ******************************************************************************/
#if (cPWR_LVD_Enable == 2)
static void PWRLib_LVD_PollIntervalCallback
(
void* param
)
{
  (void)param;
  PWRLib_LVD_SavedLevel = PWRLib_LVD_CollectLevel();
}
#endif



/*---------------------------------------------------------------------------
 * Name: PWRLib_GetSystemResetStatus
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
uint16_t PWRLib_GetSystemResetStatus
(
  void
)
{
  uint16_t resetStatus = 0;
  resetStatus = (uint16_t) (RCM->SRS0);
  resetStatus |= (uint16_t)(RCM->SRS1 << 8);
  return resetStatus;
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_Init
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_Init
(
void
)
{
#if (cPWR_UsePowerDownMode == 1)
    
    /* enable clock to LLWU module */  
#if ( (cPWR_DeepSleepMode != 0) && (cPWR_DeepSleepMode != 13) )  
    PWRLib_LLWU_UpdateWakeupReason();
#endif
    
#if ( (cPWR_DeepSleepMode == 3) || (cPWR_DeepSleepMode == 4) || (cPWR_DeepSleepMode == 6) || (cPWR_DeepSleepMode == 7) || (cPWR_DeepSleepMode == 10) || (cPWR_DeepSleepMode == 11)  || (cPWR_DeepSleepMode == 14) || (cPWR_DeepSleepMode == 15) )
    TMR_RTCInit();
#endif
    
#if ( (cPWR_DeepSleepMode == 4) || (cPWR_DeepSleepMode == 7) || (cPWR_DeepSleepMode == 11) )
    LLWU->ME = gPWRLib_LLWU_WakeupModuleEnable_RTC_Alarm_c;
#endif
    
#if ( (cPWR_DeepSleepMode == 2) || (cPWR_DeepSleepMode == 3) || (cPWR_DeepSleepMode == 5) || (cPWR_DeepSleepMode == 6) || (cPWR_DeepSleepMode == 8) || (cPWR_DeepSleepMode == 9) || (cPWR_DeepSleepMode == 10) || (cPWR_DeepSleepMode == 12) || (cPWR_DeepSleepMode == 14) || (cPWR_DeepSleepMode == 15) )  
    /* configure NVIC for LPTMR Isr */
    PWRLib_LPTMR_Init();
    /* enable LPTMR as wakeup source for LLWU module */
    LLWU->ME = gPWRLib_LLWU_WakeupModuleEnable_LPTMR_c;
#endif
    
#if ( (cPWR_DeepSleepMode != 0) && (cPWR_DeepSleepMode != 2) && (cPWR_DeepSleepMode != 3) && (cPWR_DeepSleepMode != 4) )
#if BOARD_LLWU_PIN_ENABLE_BITMAP
    {
        uint16_t pinEn16 = BOARD_LLWU_PIN_ENABLE_BITMAP;
        uint32_t PinEn32 = 0;
        uint32_t i;
        for(i=0; pinEn16 ; i++)
        {
            if(pinEn16 & 0x1)
            {
                PinEn32 |= 0x3<<(i<<1);
            }
            pinEn16 >>= 1;
        }
        LLWU->PE1 = PinEn32&0xff;
        LLWU->PE2 = (PinEn32>>8)&0xff;
        LLWU->PE3 = (PinEn32>>16)&0xff;
        LLWU->PE4 = (PinEn32>>24)&0xff;
    }
#endif 
#endif
    
#if ( (cPWR_DeepSleepMode != 0) && (cPWR_DeepSleepMode != 13) )
    /* LLWU_FILT1: FILTF=1,FILTE=0,??=0,FILTSEL=0 */
    LLWU->FILT1 = LLWU_FILT1_FILTF_MASK | LLWU_FILT1_FILTE(0x00) | LLWU_FILT1_FILTSEL(0x00);
    /* LLWU_FILT2: FILTF=1,FILTE=0,??=0,FILTSEL=0 */
    LLWU->FILT2 = LLWU_FILT2_FILTF_MASK | LLWU_FILT2_FILTE(0x00) | LLWU_FILT2_FILTSEL(0x00);
    /* SMC_PMPROT: ??=0,??=0,AVLP=1,??=0,ALLS=1,??=0,AVLLS=1,??=0 */
    SMC->PMPROT = SMC_PMPROT_AVLP_MASK | SMC_PMPROT_ALLS_MASK | SMC_PMPROT_AVLLS_MASK;  /* Setup Power mode protection register */    
    /* install LLWU Isr and validate it in NVIC */
    OSA_InstallIntHandler (LLWU_IRQn, PWRLib_LLWU_Isr);
    NVIC_SetPriority(LLWU_IRQn, 0x80 >> (8 - __NVIC_PRIO_BITS));
    NVIC_EnableIRQ(LLWU_IRQn);
    
#if (gKeyBoardSupported_d == 0 ) 
#if gLowPower_switchPinsToInitBitmap_d
    {
        uint32_t pinIndex = gLowPower_switchPinsToInitBitmap_d;
        uint32_t i;
        for(i=0 ; pinIndex ; i++)
        {
            if(pinIndex & 0x1)
            {
                (void)GpioInputPinInit(&switchPins[i], 1);
            }
            pinIndex >>= 1;
        }
    }
#endif
#endif  
    
#endif
#endif /* #if (cPWR_UsePowerDownMode==1) */
    
    /* LVD_Init TODO */
#if (cPWR_LVD_Enable == 0)
    PMC->LVDSC1 &= (uint8_t) ~( PMC_LVDSC1_LVDIE_MASK  | PMC_LVDSC1_LVDRE_MASK);
    PMC->LVDSC2 &= (uint8_t) ~( PMC_LVDSC2_LVWIE_MASK );
#elif ((cPWR_LVD_Enable == 1) || (cPWR_LVD_Enable == 2))
    PMC->LVDSC1 &= (uint8_t) ~( PMC_LVDSC1_LVDIE_MASK | PMC_LVDSC1_LVDRE_MASK);
    PMC->LVDSC2 &= (uint8_t) ~( PMC_LVDSC2_LVWIE_MASK );
#elif (cPWR_LVD_Enable==3)
    PMC->LVDSC1 = (PMC->LVDSC1 | (uint8_t)PMC_LVDSC1_LVDRE_MASK) & (uint8_t)(~PMC_LVDSC1_LVDIE_MASK );
    PMC->LVDSC2 &= (uint8_t) ~( PMC_LVDSC2_LVWIE_MASK );
#endif /* #if (cPWR_LVD_Enable) */
    
    
#if (cPWR_LVD_Enable == 2)
#if ((cPWR_LVD_Ticks == 0) || (cPWR_LVD_Ticks > 71582))
#error  "*** ERROR: cPWR_LVD_Ticks invalid value"
#endif 
    
    PWRLib_LVD_SavedLevel = PWRLib_LVD_CollectLevel(); 
    /* Allocate a platform timer */
    PWRLib_LVD_PollIntervalTmrID = TMR_AllocateTimer();   
    if(gTmrInvalidTimerID_c != PWRLib_LVD_PollIntervalTmrID)
    { 
        /* start the timer */
        TMR_StartLowPowerTimer(PWRLib_LVD_PollIntervalTmrID, gTmrIntervalTimer_c,TmrMinutes(cPWR_LVD_Ticks) , PWRLib_LVD_PollIntervalCallback, NULL); 
    }
#endif  /* #if (cPWR_LVD_Enable==2) */
}

/*---------------------------------------------------------------------------
 * Name: PWRLib_Reset
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void PWRLib_Reset
(
  void
)
{
  NVIC_SystemReset();
  while(1);
}
