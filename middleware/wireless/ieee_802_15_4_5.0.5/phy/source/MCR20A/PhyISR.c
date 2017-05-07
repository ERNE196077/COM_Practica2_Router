/*!
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* All rights reserved.
*
* \file
* PHY ISR Functions
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
#include "board.h"
#include "MCR20Drv.h"
#include "MCR20Reg.h"
#include "Phy.h"
#include "PhyInterface.h"
#include "GPIO_Adapter.h"
#include "gpio_pins.h"
#include "fsl_os_abstraction.h"
#include "MpmInterface.h"

#ifndef gMWS_UseCoexistence_d
#define gMWS_UseCoexistence_d 0
#endif

#if gMWS_UseCoexistence_d
#include "MWS.h"
#endif

/*! *********************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
********************************************************************************** */
#ifndef MCR20_Irq_Priority
#if defined(MCU_MKL46Z4)
  #define MCR20_Irq_Priority     (0xC0)
#else
  #define MCR20_Irq_Priority     (0x80)
#endif
#endif

#if gPhyUseReducedSpiAccess_d
#define mPhyWakeIrqCB_c        (1<<0)
#define mPhyFilterFailIrqCB_c  (1<<1)
#define mPhyRxWatermarkIrqCB_c (1<<2)
#define mPhyTmr1IrqCB_c        (1<<3)
#define mPhyTmr2IrqCB_c        (1<<4)
#define mPhyTmr3IrqCB_c        (1<<5)
#define mPhyTmr4IrqCB_c        (1<<6)
#define mPhySeqIrqCB_c         (1<<7)
#endif


/*! *********************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
********************************************************************************** */
extern Phy_PhyLocalStruct_t     phyLocal[];
static volatile phyRxParams_t * mpRxParams = NULL;
static uint32_t                 mPhyTaskInstance;

/* Mirror XCVR control registers */
uint8_t                         mStatusAndControlRegs[9];
uint8_t                         mPhyLastRxLQI = 0;
uint8_t                         mPhyLastRxRSSI = 0;
bool_t                          mPhyForceFP = FALSE;

#if gPhyRxPBTransferThereshold_d
uint8_t mPhyWatermarkLevel;
#define mPhyGetPBTransferThreshold(len) ((len) - 2)
#endif

#if gMWS_UseCoexistence_d
extern uint8_t mCoexAbortPending;
#endif


/*! *********************************************************************************
*************************************************************************************
* Private prototypes
*************************************************************************************
********************************************************************************** */
#if gPhyRxPBTransferThereshold_d
static void PreprocessPollReq(uint8_t*);
#endif

#if gPhyUseReducedSpiAccess_d
static void Phy_RestartRxFromISR();
#else
static void Phy_GetRxParams(void);
#endif

#if gMWS_UseCoexistence_d
static void Phy_CoexAbort(Phy_PhyLocalStruct_t *pPhyStruct);
#endif


/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
* \brief  Sets the current PHY instance waiting for an IRQ
*
* \param[in]  instanceId instance of the PHY
*
********************************************************************************** */
void PhyIsrPassTaskParams
(
  instanceId_t instanceId
)
{
    mPhyTaskInstance = instanceId;
}

/*! *********************************************************************************
* \brief  Sets the location of the Rx parameters
*
* \param[in]  pRxParam pointer to Rx parameters
*
********************************************************************************** */
void PhyIsrPassRxParams
(
  volatile phyRxParams_t * pRxParam
)
{
    mpRxParams = pRxParam;
}

/*! *********************************************************************************
* \brief  Clear and mask PHY IRQ, set sequence to Idle
*
********************************************************************************** */
void PhyIsrSeqCleanup
(
  void
)
{
    mStatusAndControlRegs[IRQSTS1] |= 0x0F; /* clear all Seq IRQ flags */
    mStatusAndControlRegs[PHY_CTRL1] &= ~(cPHY_CTRL1_XCVSEQ);
    mStatusAndControlRegs[PHY_CTRL1] &= ~(cPHY_CTRL1_TMRTRIGEN); /* disable autosequence start by TC2 match */
    mStatusAndControlRegs[PHY_CTRL2] |= (cPHY_CTRL2_CCAMSK | \
                                         cPHY_CTRL2_RXMSK  | \
                                         cPHY_CTRL2_TXMSK  | \
                                         cPHY_CTRL2_SEQMSK );

#if gPhyUseReducedSpiAccess_d
    mStatusAndControlRegs[IRQSTS3] |= cIRQSTS3_TMR3MSK; /* mask TMR3 interrupt */
#else
    mStatusAndControlRegs[IRQSTS3] &= 0xF0;
    mStatusAndControlRegs[IRQSTS3] |= cIRQSTS3_TMR3MSK; /* mask TMR3 interrupt */

    /* Clear transceiver interrupts, mask SEQ, RX, TX and CCA interrupts
       and set the PHY sequencer back to IDLE */
    MCR20Drv_DirectAccessSPIMultiByteWrite(IRQSTS1, mStatusAndControlRegs, 5);
#endif
}

/*! *********************************************************************************
* \brief  Clear and mask PHY IRQ, disable timeout, set sequence to Idle
*
********************************************************************************** */
void PhyIsrTimeoutCleanup
(
  void
)
{
#if !gPhyUseReducedSpiAccess_d

    mStatusAndControlRegs[IRQSTS3] &= 0xF0;
    mStatusAndControlRegs[IRQSTS3] |= (cIRQSTS3_TMR3MSK | \
                                                   cIRQSTS3_TMR3IRQ); /* mask and clear TMR3 interrupt */
    mStatusAndControlRegs[PHY_CTRL1]   &= ~(cPHY_CTRL1_XCVSEQ);
    mStatusAndControlRegs[PHY_CTRL1]   &= ~(cPHY_CTRL1_TMRTRIGEN); /* disable autosequence start by TC2 match */
    mStatusAndControlRegs[PHY_CTRL2]   |=  (cPHY_CTRL2_CCAMSK | \
                                            cPHY_CTRL2_RXMSK  | \
                                            cPHY_CTRL2_TXMSK  | \
                                            cPHY_CTRL2_SEQMSK);

    /* Disable TMR3 comparator and Sequence timeout */
    mStatusAndControlRegs[PHY_CTRL3]   &= ~(cPHY_CTRL3_TMR3CMP_EN);
    mStatusAndControlRegs[PHY_CTRL4]   &= ~(cPHY_CTRL4_TC3TMOUT);

    /* Clear transceiver interrupts, mask mask SEQ, RX, TX, TMR3 and CCA interrupts interrupts
       and set the PHY sequencer back to IDLE */
    MCR20Drv_DirectAccessSPIMultiByteWrite(IRQSTS1, mStatusAndControlRegs, 8);
#endif
}

/*! *********************************************************************************
* \brief  Scales energy level to 0-255
*
* \param[in]  energyLevel  the energ level reported by HW
*
* \return  uint8_t  the energy level scaled in 0x00-0xFF
*
********************************************************************************** */
uint8_t Phy_GetEnergyLevel
(
uint8_t energyLevel /* db */
)
{
    if(energyLevel >= 90)
    {
        /* ED value is below minimum. Return 0x00. */
        energyLevel = 0x00;
    }
    else if(energyLevel <= 26)
    {
        /* ED value is above maximum. Return 0xFF. */
        energyLevel = 0xFF;
    }
    else
    {
        /* Energy level (-90 dBm to -26 dBm ) --> varies form 0 to 64 */
        energyLevel = (90 - energyLevel);
        /* Rescale the energy level values to the 0x00-0xff range (0 to 64 translates in 0 to 255) */
        /* energyLevel * 3.9844 ~= 4 */
        /* Multiply with 4=2^2 by shifting left.
        The multiplication will not overflow beacause energyLevel has values between 0 and 63 */
        energyLevel <<= 2;
    }

    return energyLevel;
}

/*! *********************************************************************************
* \brief  Scales LQI to 0-255
*
* \param[in]  hwLqi  the LQI reported by HW
*
* \return  uint8_t  the LQI scaled in 0x00-0xFF
*
********************************************************************************** */
static uint8_t Phy_LqiConvert
(
uint8_t hwLqi
)
{
    uint32_t tmpLQI;

    /* LQI Saturation Level */
    if (hwLqi >= 230)
    {
        return 0xFF;
    }
    else if (hwLqi <= 9)
    {
        return 0;
    }
    else
    {
        /* Rescale the LQI values from min to saturation to the 0x00 - 0xFF range */
        /* The LQI value mst be multiplied by ~1.1087 */
        /* tmpLQI =  hwLqi * 7123 ~= hwLqi * 65536 * 0.1087 = hwLqi * 2^16 * 0.1087*/
        tmpLQI = ((uint32_t)hwLqi * (uint32_t)7123 );
        /* tmpLQI =  (tmpLQI / 2^16) + hwLqi */
        tmpLQI = (uint32_t)(tmpLQI >> 16) + (uint32_t)hwLqi;

        return (uint8_t)tmpLQI;
    }
}

/*! *********************************************************************************
* \brief  This function returns the LQI for the las received packet
*
* \return  uint8_t  LQI value
*
********************************************************************************** */
uint8_t PhyGetLastRxLqiValue(void)
{
    return mPhyLastRxLQI;
}

/*! *********************************************************************************
* \brief  This function converts the LQI reported by the PHY into an signed RSSI value
*
* \param[in]  LQI  the LQI reported by the PHY
*
* \return  the RSSI value in dbm
*
********************************************************************************** */
int8_t PhyConvertLQIToRSSI(uint8_t lqi)
{
    int32_t rssi = (50*lqi - 16820) / 163;
    return (int8_t)rssi;
}

/*! *********************************************************************************
* \brief  This function returns the RSSI for the las received packet
*
* \return  uint8_t  RSSI value
*
********************************************************************************** */
uint8_t PhyGetLastRxRssiValue(void)
{
  uint32_t tempRSSI = mPhyLastRxRSSI;
  uint8_t comp = MCR20Drv_IndirectAccessSPIRead(LQI_OFFSET_COMP);
  tempRSSI += comp;
  tempRSSI >>= 1;
  comp >>=1;
  if(25*(tempRSSI+comp) > 4360)
  {
    return mPhyLastRxRSSI;
  }
  /*liniarization
            4360 - 25* RSSI      (4360 - 25* RSSI)*7085 >> 18;
  abs(rssi)=---------------  <=>
                   37
  */
  tempRSSI = ((4360 - 25*(tempRSSI + comp))*7085)>>18;
  return (uint8_t)(0x000000FF & tempRSSI);
}

/*! *********************************************************************************
* \brief  PHY ISR
*
********************************************************************************** */
#if gPhyUseReducedSpiAccess_d
void PHY_InterruptHandler(void)
{
    uint8_t xcvseqCopy;
    uint8_t irqStatus;
    uint8_t spiWriteLen = 1;
    uint32_t cbMask = 0;
    Phy_PhyLocalStruct_t *pPhyStruct = &phyLocal[mPhyTaskInstance];

    /* Clear transceiver(IRQ_B) interrupt */
    MCR20Drv_IRQ_Clear();
    
#if gMWS_UseCoexistence_d
    if( mCoexAbortPending )
    {
        Phy_CoexAbort(pPhyStruct);
    }
#endif

    /* Read transceiver interrupt status and control registers */
    irqStatus = MCR20Drv_DirectAccessSPIMultiByteRead(IRQSTS2, &mStatusAndControlRegs[1], 7);
    mStatusAndControlRegs[IRQSTS1] = irqStatus & 0xF0;
    xcvseqCopy = mStatusAndControlRegs[PHY_CTRL1] & cPHY_CTRL1_XCVSEQ;

    /* Wake IRQ */
    if( !(mStatusAndControlRegs[PHY_CTRL3] & cPHY_CTRL3_WAKE_MSK) &&
         (mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_WAKE_IRQ) )
    {
        mStatusAndControlRegs[PHY_CTRL3] |= cPHY_CTRL3_WAKE_MSK;
        MCR20Drv_DirectAccessSPIWrite(PHY_CTRL3, mStatusAndControlRegs[PHY_CTRL3]);
        cbMask |= mPhyWakeIrqCB_c;
    }

    /* Flter Fail IRQ */
    if( !(mStatusAndControlRegs[PHY_CTRL2] & cPHY_CTRL2_FILTERFAIL_MSK) &&
         (irqStatus & cIRQSTS1_FILTERFAIL_IRQ) )
    {
        cbMask |= mPhyFilterFailIrqCB_c;
    }
    /* Rx Watermark IRQ */
    if( !(mStatusAndControlRegs[PHY_CTRL2] & cPHY_CTRL2_RX_WMRK_MSK) &&
         (irqStatus & cIRQSTS1_RXWTRMRKIRQ) )
    {
        cbMask |= mPhyRxWatermarkIrqCB_c;
    }

    /* Timer 1 Compare Match */
    if( (mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR1IRQ) &&
       !(mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR1MSK))
    {
        mStatusAndControlRegs[IRQSTS3] |= cIRQSTS3_TMR1MSK;
        cbMask |= mPhyTmr1IrqCB_c;
        spiWriteLen = 3;
    }

    /* Timer 2 Compare Match */
    if( !(mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR2MSK) &&
         (mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR2IRQ) )
    {
        mStatusAndControlRegs[IRQSTS3] |= cIRQSTS3_TMR2MSK;
        cbMask |= mPhyTmr2IrqCB_c;
        spiWriteLen = 3;
    }

    /* Timer 3 Compare Match */
    if( !(mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR3MSK) &&
         (mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR3IRQ) )
    {
        mStatusAndControlRegs[IRQSTS3] |= cIRQSTS3_TMR3MSK;
        spiWriteLen = 3;

        /* Sequence time-out! Abort current sequence */
        if(gIdle_c == xcvseqCopy)
        {
            cbMask |= mPhyTmr3IrqCB_c;
        }
        else if( ((gTR_c == xcvseqCopy) || (gRX_c == xcvseqCopy)) &&
                !(irqStatus & cIRQSTS1_RXIRQ) )
        {
            cbMask |= mPhyTmr3IrqCB_c | mPhySeqIrqCB_c;
            PhyIsrSeqCleanup();
            spiWriteLen = 5;
        }
    }

    /* Timer 4 Compare Match */
    if( !(mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR4MSK) &&
         (mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR4IRQ) )
    {
        mStatusAndControlRegs[IRQSTS3] |= cIRQSTS3_TMR4MSK;
        cbMask |= mPhyTmr4IrqCB_c;
        spiWriteLen = 3;
    }

    /* Sequencer interrupt, the autosequence has completed */
    if( !(mStatusAndControlRegs[PHY_CTRL2] & cPHY_CTRL2_SEQMSK) &&
         (irqStatus & cIRQSTS1_SEQIRQ) )
    {
        PhyIsrSeqCleanup();
        cbMask |= mPhySeqIrqCB_c;
        spiWriteLen = 5;
    }

    /* Synchronize XCVR settings. This will clear all interrupts flags (w1tc) */
    MCR20Drv_DirectAccessSPIMultiByteWrite(IRQSTS1, mStatusAndControlRegs, spiWriteLen);

    /* Callbacks */
    /***********************************************************************************/
    if (cbMask & mPhyWakeIrqCB_c)
    {
#ifdef MAC_PHY_DEBUG
        Radio_Phy_UnexpectedTransceiverReset(mPhyTaskInstance);
#endif
        Radio_Phy_Notify(mPhyTaskInstance);
        return;
    }

    if (cbMask & mPhyFilterFailIrqCB_c)
    {
#if gPhyRxPBTransferThereshold_d
        /* Reset the RX_WTR_MARK level since packet was dropped. */
        if( mPhyWatermarkLevel )
        {
            mPhyWatermarkLevel = 0;
            MCR20Drv_IndirectAccessSPIWrite(RX_WTR_MARK, mPhyWatermarkLevel);
        }
#endif
        if( (xcvseqCopy == gRX_c) && !(cbMask & mPhySeqIrqCB_c) )
        {
            // Abort current SEQ
            mStatusAndControlRegs[PHY_CTRL1] &= ~(cPHY_CTRL1_XCVSEQ);
            MCR20Drv_DirectAccessSPIWrite(PHY_CTRL1, mStatusAndControlRegs[PHY_CTRL1]);
            // wait for Sequence Idle
            while ((MCR20Drv_DirectAccessSPIRead(SEQ_STATE) & 0x1F) != 0);
            while( !(MCR20Drv_DirectAccessSPIRead(IRQSTS1) & cIRQSTS1_SEQIRQ) );
            // Clear IRQ flags:
            MCR20Drv_DirectAccessSPIWrite(IRQSTS1, cIRQSTS1_SEQIRQ);
            // Restart Rx asap
            mStatusAndControlRegs[PHY_CTRL1] |= gRX_c;
            MCR20Drv_DirectAccessSPIWrite(PHY_CTRL1, mStatusAndControlRegs[PHY_CTRL1]);
        }
        Radio_Phy_PlmeFilterFailRx(mPhyTaskInstance);
    }
    else if (cbMask & mPhyRxWatermarkIrqCB_c)
    {
#if gPhyRxPBTransferThereshold_d
        if( 0 == mPhyWatermarkLevel )
        {
            /* Check if this is a standalone RX because we could end up here during a TR sequence also. */
            if( xcvseqCopy == gRX_c )
            {
                if( cbMask & mPhySeqIrqCB_c )
                {
                    if( mpRxParams && (mStatusAndControlRegs[RX_FRM_LEN] <= 127) )
                    {
                        mpRxParams->pRxData->msgData.dataInd.pPsdu = (uint8_t*)&mpRxParams->pRxData->msgData.dataInd.pPsdu + sizeof(mpRxParams->pRxData->msgData.dataInd.pPsdu);
                        MCR20Drv_PB_SPIBurstRead(mpRxParams->pRxData->msgData.dataInd.pPsdu, (uint8_t)(mStatusAndControlRegs[RX_FRM_LEN] - 2));
                    }
                }
                else
                {
                    /* Set the thereshold packet length at which to start the PB Burst Read.*/
                    mPhyWatermarkLevel = mPhyGetPBTransferThreshold( mStatusAndControlRegs[RX_FRM_LEN] );
                    MCR20Drv_IndirectAccessSPIWrite(RX_WTR_MARK, mPhyWatermarkLevel);
                }
            }
#endif
        Radio_Phy_PlmeRxSfdDetect(mPhyTaskInstance, mStatusAndControlRegs[RX_FRM_LEN]);
#if gPhyRxPBTransferThereshold_d
        }
        else
        {
            if( mpRxParams && (mStatusAndControlRegs[RX_FRM_LEN] <= 127) )
            {
                // Read data from PB
            	mPhyLastRxRSSI = MCR20Drv_DirectAccessSPIRead(LQI_VALUE);
                mpRxParams->pRxData->msgData.dataInd.pPsdu = (uint8_t*)&mpRxParams->pRxData->msgData.dataInd.pPsdu + sizeof(mpRxParams->pRxData->msgData.dataInd.pPsdu);
                MCR20Drv_PB_SPIBurstRead(mpRxParams->pRxData->msgData.dataInd.pPsdu, (uint8_t)(mStatusAndControlRegs[RX_FRM_LEN] - 2));

                if( (mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_PI) &&      /* This is a Poll packet */
                   !(mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_SRCADDR) )  /* No data pending for the device */
                {
                    PreprocessPollReq(mpRxParams->pRxData->msgData.dataInd.pPsdu);
                }
            }

            /* Reset RX_WTR_MARK here, because if the FCS fails, no other IRQ will arrive
            * and the RX will restart automatically. */
            mPhyWatermarkLevel = 0;
            MCR20Drv_IndirectAccessSPIWrite(RX_WTR_MARK, mPhyWatermarkLevel);
        }
#endif
    }

    if (cbMask & mPhyTmr1IrqCB_c)
    {
        Radio_Phy_TimeWaitTimeoutIndication(mPhyTaskInstance);
    }

    if ((cbMask & mPhyTmr2IrqCB_c) && (gIdle_c != xcvseqCopy))
    {
        Radio_Phy_TimeStartEventIndication(mPhyTaskInstance);
    }

//    if (cbMask & mPhyTmr4IrqCB_c)
//    {
//    }

    if (cbMask & mPhySeqIrqCB_c)
    {
#if gPhyRxPBTransferThereshold_d
        if( mPhyWatermarkLevel )
        {
            /* The RX_WTR_MARK level is out of sync with the current sequence. Reset to 0! */
            mPhyWatermarkLevel = 0;
            MCR20Drv_IndirectAccessSPIWrite(RX_WTR_MARK, mPhyWatermarkLevel);
        }
#endif

        if( irqStatus & cIRQSTS1_PLL_UNLOCK_IRQ )
        {
#if gMWS_UseCoexistence_d
            MWS_CoexistenceReleaseAccess();
#endif
            Radio_Phy_PlmeSyncLossIndication(mPhyTaskInstance);
        }
        else if (cbMask & mPhyTmr3IrqCB_c)
        {
#if gMWS_UseCoexistence_d
            MWS_CoexistenceReleaseAccess();
#endif
            Radio_Phy_TimeRxTimeoutIndication(mPhyTaskInstance);
        }
        else
        {
            switch(xcvseqCopy)
            {
            case gTX_c:
#if gMWS_UseCoexistence_d
                MWS_CoexistenceReleaseAccess();
#endif
                Phy_RestartRxFromISR();
                if( (mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_CCA) &&
                    (mStatusAndControlRegs[PHY_CTRL1]   & cPHY_CTRL1_CCABFRTX) )
                {
                    Radio_Phy_PlmeCcaConfirm(gPhyChannelBusy_c, mPhyTaskInstance);
                }
                else
                {
                    Radio_Phy_PdDataConfirm(mPhyTaskInstance, FALSE);
                }
                break;

            case gTR_c:
#if gMWS_UseCoexistence_d
                MWS_CoexistenceReleaseAccess();
#endif
                if( (mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_CCA) &&
                    (mStatusAndControlRegs[PHY_CTRL1]   & cPHY_CTRL1_CCABFRTX) )
                {
                    Phy_RestartRxFromISR();
                    Radio_Phy_PlmeCcaConfirm(gPhyChannelBusy_c, mPhyTaskInstance);
                }
                else
                {
                    mPhyLastRxRSSI = MCR20Drv_DirectAccessSPIRead(LQI_VALUE);

                    Phy_RestartRxFromISR();

                    if(NULL != mpRxParams)
                    {
                        mpRxParams->psduLength = mStatusAndControlRegs[RX_FRM_LEN]; //Including FCS (2 bytes)
                        mpRxParams->linkQuality = Phy_LqiConvert(mPhyLastRxRSSI);
                        mPhyLastRxLQI = mpRxParams->linkQuality;
                        MCR20Drv_DirectAccessSPIMultiByteRead(TIMESTAMP_LSB, (uint8_t *)&mpRxParams->timeStamp, 3);
                    }

                    if( (irqStatus & cIRQSTS1_RX_FRM_PEND) == cIRQSTS1_RX_FRM_PEND )
                    {
                        Radio_Phy_PdDataConfirm(mPhyTaskInstance, TRUE);
                    }
                    else
                    {
                        Radio_Phy_PdDataConfirm(mPhyTaskInstance, FALSE);
                    }
                }
                break;

            case gRX_c:
#if gMWS_UseCoexistence_d
                MWS_CoexistenceReleaseAccess();
#endif
                if (mStatusAndControlRegs[RX_FRM_LEN] <= 127)
                {
                    mpRxParams->pDataPending = mpRxParams->pRxData;
                    mpRxParams->pRxData = NULL;
                }
#if !gPhyRxPBTransferThereshold_d
                mPhyLastRxRSSI = MCR20Drv_DirectAccessSPIRead(LQI_VALUE);
#endif
                
                Phy_RestartRxFromISR();

                if(NULL != mpRxParams)
                {
                    mpRxParams->psduLength = mStatusAndControlRegs[RX_FRM_LEN]; //Including FCS (2 bytes)
                    mpRxParams->linkQuality = Phy_LqiConvert(mPhyLastRxRSSI);
                    mPhyLastRxLQI = mpRxParams->linkQuality;
                    MCR20Drv_DirectAccessSPIMultiByteRead(TIMESTAMP_LSB, (uint8_t *)&mpRxParams->timeStamp, 3);
                }

                if( mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_PI )
                {
                    if( mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_SRCADDR )
                    {
                        pPhyStruct->flags |= gPhyFlagTxAckFP_c;
                    }
                    else
                    {
                        pPhyStruct->flags &= ~gPhyFlagTxAckFP_c;
                    }
                }
                
                Radio_Phy_PdDataIndication(mPhyTaskInstance);
                break;

            case gCCA_c:
                if( (mStatusAndControlRegs[PHY_CTRL4] & (cPHY_CTRL4_CCATYPE << cPHY_CTRL4_CCATYPE_Shift_c)) == (gCcaED_c << cPHY_CTRL4_CCATYPE_Shift_c) )
                {
                    // Ed
#if gMWS_UseCoexistence_d
                    MWS_CoexistenceReleaseAccess();
#endif
                    uint8_t temp = MCR20Drv_DirectAccessSPIRead(CCA1_ED_FNL);
                    temp = MCR20Drv_DirectAccessSPIRead(CCA1_ED_FNL);
                    Radio_Phy_PlmeEdConfirm(temp, mPhyTaskInstance);
                }
                else
                {
                    // CCA
                    if( mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_CCA )
                    {
#if gMWS_UseCoexistence_d
                        MWS_CoexistenceReleaseAccess();
#endif
#if gUseStandaloneCCABeforeTx_d
                        pPhyStruct->txParams.numOfCca = 0;
#endif
                        Phy_RestartRxFromISR();
                        Radio_Phy_PlmeCcaConfirm(gPhyChannelBusy_c, mPhyTaskInstance);
                    }
                    else
                    {
#if gUseStandaloneCCABeforeTx_d
                        if( pPhyStruct->txParams.numOfCca > 0 )
                        {
                            uint8_t newState;
                            if( --pPhyStruct->txParams.numOfCca == 0 )
                            {
                                /* Perform TxRxAck sequence if required by phyTxMode */
                                if( gPhyRxAckRqd_c == pPhyStruct->txParams.ackRequired )
                                {
                                    mStatusAndControlRegs[PHY_CTRL1] |= (uint8_t) (cPHY_CTRL1_RXACKRQD);
                                    newState = gTR_c;
                                }
                                else
                                {
                                    mStatusAndControlRegs[PHY_CTRL1] &= (uint8_t) ~(cPHY_CTRL1_RXACKRQD);
                                    newState = gTX_c;
                                }
                            }
                            else
                            {
                                newState = gCCA_c;
                            }
                            /* Write new seq */
                            mStatusAndControlRegs[PHY_CTRL1] &= (uint8_t) ~(cPHY_CTRL1_XCVSEQ);
                            mStatusAndControlRegs[PHY_CTRL1] |= newState;
                            /* Unmask SEQ interrupt */
                            mStatusAndControlRegs[PHY_CTRL2] &= (uint8_t) ~(cPHY_CTRL2_SEQMSK);
                            /* Start the sequence immediately */
                            MCR20Drv_DirectAccessSPIMultiByteWrite(PHY_CTRL1,
                                                                   &mStatusAndControlRegs[PHY_CTRL1],
                                                                   2);
                            if( newState == gTR_c )
                            {
                                phyTime_t timeout;
                                macToPdDataMessage_t *pPD = (macToPdDataMessage_t*)pPhyStruct->pReq;

                                PhyTimeReadClock(&timeout);
                                timeout += gPhyWarmUpTime_c + gPhySHRDuration_c + 54 +
                                    (pPD->msgData.dataReq.psduLength + 1) * gPhySymbolsPerOctet_c;
                                PhyTimeSetEventTimeout(&timeout);
                            }
                        }
                        else
#endif
                        {
#if gMWS_UseCoexistence_d
                            MWS_CoexistenceReleaseAccess();
#endif
                            Radio_Phy_PlmeCcaConfirm(gPhyChannelIdle_c, mPhyTaskInstance);
                        }
                    }
                }
                break;

            case gCCCA_c:
#if gMWS_UseCoexistence_d
                MWS_CoexistenceReleaseAccess();
#endif
                Radio_Phy_PlmeCcaConfirm(gPhyChannelIdle_c, mPhyTaskInstance);
                break;

            default:
#if gMWS_UseCoexistence_d
                MWS_CoexistenceReleaseAccess();
#endif
                Radio_Phy_PlmeSyncLossIndication(mPhyTaskInstance);
                break;
            }
        }
    }
    else if (cbMask & mPhyTmr3IrqCB_c)
    {
        Radio_Phy_TimeRxTimeoutIndication(mPhyTaskInstance);
    }

    Radio_Phy_Notify(mPhyTaskInstance);
}

#else
void PHY_InterruptHandler(void)
{
    Phy_PhyLocalStruct_t *pPhyStruct = &phyLocal[mPhyTaskInstance];
    uint8_t xcvseqCopy;

    /* clear transceiver(IRQ_B) interrupt */
    MCR20Drv_IRQ_Clear();
    
#if gMWS_UseCoexistence_d
    if( mCoexAbortPending )
    {
        Phy_CoexAbort(pPhyStruct);
    }
#endif

    /* Read transceiver interrupt status and control registers */
    mStatusAndControlRegs[IRQSTS1] =
        MCR20Drv_DirectAccessSPIMultiByteRead(IRQSTS2, &mStatusAndControlRegs[1], 7);
    xcvseqCopy = mStatusAndControlRegs[PHY_CTRL1] & cPHY_CTRL1_XCVSEQ;
    /* clear transceiver interrupts */
    MCR20Drv_DirectAccessSPIMultiByteWrite(IRQSTS1, mStatusAndControlRegs, 3);

    if( (mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_WAKE_IRQ) &&
       !(mStatusAndControlRegs[PHY_CTRL3] & cPHY_CTRL3_WAKE_MSK) )
    {
#ifdef MAC_PHY_DEBUG
        Radio_Phy_UnexpectedTransceiverReset(mPhyTaskInstance);
#endif
        Radio_Phy_Notify(mPhyTaskInstance);
        return;
    }

    /* Flter Fail IRQ */
    if( (mStatusAndControlRegs[IRQSTS1] & cIRQSTS1_FILTERFAIL_IRQ) &&
       !(mStatusAndControlRegs[PHY_CTRL2] & cPHY_CTRL2_FILTERFAIL_MSK) )
    {
        if( (xcvseqCopy == gRX_c) && !(mStatusAndControlRegs[IRQSTS1] & cIRQSTS1_SEQIRQ) )
        {
            // Abort current SEQ
            mStatusAndControlRegs[PHY_CTRL1] &= ~(cPHY_CTRL1_XCVSEQ);
            MCR20Drv_DirectAccessSPIWrite(PHY_CTRL1, mStatusAndControlRegs[PHY_CTRL1]);
            // wait for Sequence Idle
            while ((MCR20Drv_DirectAccessSPIRead(SEQ_STATE) & 0x1F) != 0);
            // Clear IRQ flags:
            MCR20Drv_DirectAccessSPIWrite(IRQSTS1, cIRQSTS1_SEQIRQ);
            // Restart Rx asap
            mStatusAndControlRegs[PHY_CTRL1] |= gRX_c;
            MCR20Drv_DirectAccessSPIWrite(PHY_CTRL1, mStatusAndControlRegs[PHY_CTRL1]);
        }

#if gPhyRxPBTransferThereshold_d
        /* Reset the RX_WTR_MARK level since packet was dropped. */
        mPhyWatermarkLevel = 0;
        MCR20Drv_IndirectAccessSPIWrite(RX_WTR_MARK, mPhyWatermarkLevel);
#endif
        Radio_Phy_PlmeFilterFailRx(mPhyTaskInstance);
    }
    /* Rx Watermark IRQ */
    else if( (mStatusAndControlRegs[IRQSTS1] & cIRQSTS1_RXWTRMRKIRQ) &&
            !(mStatusAndControlRegs[PHY_CTRL2] & cPHY_CTRL2_RX_WMRK_MSK) )
    {
#if gPhyRxPBTransferThereshold_d
        if( 0 == mPhyWatermarkLevel )
        {
            /* Check if this is a standalone RX because we could end up here during a TR sequence also. */
            if( xcvseqCopy == gRX_c )
            {
                if( mStatusAndControlRegs[IRQSTS1] & cIRQSTS1_SEQIRQ )
                {
                    if( mpRxParams && (mStatusAndControlRegs[RX_FRM_LEN] <= 127) )
                    {
                        /* Read data from PB */
                        mpRxParams->pRxData->msgData.dataInd.pPsdu = (uint8_t*)&mpRxParams->pRxData->msgData.dataInd.pPsdu + 
                                                                 sizeof(mpRxParams->pRxData->msgData.dataInd.pPsdu);
                        MCR20Drv_PB_SPIBurstRead(mpRxParams->pRxData->msgData.dataInd.pPsdu, 
                                             mStatusAndControlRegs[RX_FRM_LEN] - 2);
                    }
                }
                else
                {
                    /* Set the thereshold packet length at which to start the PB Burst Read.*/
                    mPhyWatermarkLevel = mPhyGetPBTransferThreshold( mStatusAndControlRegs[RX_FRM_LEN] );
                    MCR20Drv_IndirectAccessSPIWrite(RX_WTR_MARK, mPhyWatermarkLevel);
                }
            }
#endif
            Radio_Phy_PlmeRxSfdDetect(mPhyTaskInstance, mStatusAndControlRegs[RX_FRM_LEN]);
#if gPhyRxPBTransferThereshold_d
        }
        else
        {
            if( mpRxParams && (mStatusAndControlRegs[RX_FRM_LEN] <= 127))
            {
                /* Read data from PB */
                mpRxParams->pRxData->msgData.dataInd.pPsdu = (uint8_t*)&mpRxParams->pRxData->msgData.dataInd.pPsdu + sizeof(mpRxParams->pRxData->msgData.dataInd.pPsdu);
                MCR20Drv_PB_SPIBurstRead(mpRxParams->pRxData->msgData.dataInd.pPsdu, (uint8_t)(mStatusAndControlRegs[RX_FRM_LEN] - 2));
                if( (mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_PI) &&      /* This is a Poll packet */
                   !(mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_SRCADDR) )  /* No data pending for the device */
                {
                    PreprocessPollReq(mpRxParams->pRxData->msgData.dataInd.pPsdu);
                }
            }
            /* Reset RX_WTR_MARK here, because if the FCS fails, no other IRQ will arrive
             * and the RX will restart automatically. */
            mPhyWatermarkLevel = 0;
            MCR20Drv_IndirectAccessSPIWrite(RX_WTR_MARK, mPhyWatermarkLevel);
        }
#endif
    }

    /* Timer 1 Compare Match */
    if( (mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR1IRQ) &&
       !(mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR1MSK))
    {
        /* Disable TMR1 comparator */
        mStatusAndControlRegs[PHY_CTRL3]   &= (uint8_t) ~( cPHY_CTRL3_TMR1CMP_EN);
        MCR20Drv_DirectAccessSPIWrite(PHY_CTRL3, mStatusAndControlRegs[PHY_CTRL3]);

        Radio_Phy_TimeWaitTimeoutIndication(mPhyTaskInstance);
    }

    /* Sequencer interrupt, the autosequence has completed */
    if( (mStatusAndControlRegs[IRQSTS1] & cIRQSTS1_SEQIRQ) &&
       !(mStatusAndControlRegs[PHY_CTRL2] & cPHY_CTRL2_SEQMSK) )
    {
#if gPhyRxPBTransferThereshold_d
        if( mPhyWatermarkLevel )
        {
            /* The RX_WTR_MARK level is out of sync with the current sequence. Reset to 0! */
            mPhyWatermarkLevel = 0;
            MCR20Drv_IndirectAccessSPIWrite(RX_WTR_MARK, mPhyWatermarkLevel);
        }
#endif

        /* PLL unlock, the autosequence has been aborted due to PLL unlock */
        if( mStatusAndControlRegs[IRQSTS1] & cIRQSTS1_PLL_UNLOCK_IRQ )
        {
#if gMWS_UseCoexistence_d
            MWS_CoexistenceReleaseAccess();
#endif
            PhyIsrSeqCleanup();
            Radio_Phy_PlmeSyncLossIndication(mPhyTaskInstance);
            Radio_Phy_Notify(mPhyTaskInstance);
            return;
        }

        /* TMR3 timeout, the autosequence has been aborted due to TMR3 timeout */
        if( (mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR3IRQ) &&
           !(mStatusAndControlRegs[IRQSTS1] & cIRQSTS1_RXIRQ) &&
            (gTX_c != xcvseqCopy) )
        {
#if gMWS_UseCoexistence_d
            MWS_CoexistenceReleaseAccess();
#endif
            PhyIsrTimeoutCleanup();

            Radio_Phy_TimeRxTimeoutIndication(mPhyTaskInstance);
            Radio_Phy_Notify(mPhyTaskInstance);
            return;
        }

        PhyIsrSeqCleanup();

        switch(xcvseqCopy)
        {
        case gTX_c:
#if gMWS_UseCoexistence_d
            MWS_CoexistenceReleaseAccess();
#endif
            if( (mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_CCA) &&
                (mStatusAndControlRegs[PHY_CTRL1]   & cPHY_CTRL1_CCABFRTX) )
            {
                Radio_Phy_PlmeCcaConfirm(gPhyChannelBusy_c, mPhyTaskInstance);
            }
            else
            {
                Radio_Phy_PdDataConfirm(mPhyTaskInstance, FALSE);
            }
            break;

        case gTR_c:
#if gMWS_UseCoexistence_d
            MWS_CoexistenceReleaseAccess();
#endif
            if( (mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_CCA) &&
                (mStatusAndControlRegs[PHY_CTRL1]   & cPHY_CTRL1_CCABFRTX) )
            {
                Radio_Phy_PlmeCcaConfirm(gPhyChannelBusy_c, mPhyTaskInstance);
            }
            else
            {
                if( (mStatusAndControlRegs[IRQSTS1] & cIRQSTS1_RX_FRM_PEND) == cIRQSTS1_RX_FRM_PEND )
                {
                    Radio_Phy_PdDataConfirm(mPhyTaskInstance, TRUE);
                }
                else
                {
                    Radio_Phy_PdDataConfirm(mPhyTaskInstance, FALSE);
                }
            }
            break;

        case gRX_c:
#if gMWS_UseCoexistence_d
            MWS_CoexistenceReleaseAccess();
#endif
            if (mStatusAndControlRegs[RX_FRM_LEN] <= 127)
            {
                mpRxParams->pDataPending = mpRxParams->pRxData;
                mpRxParams->pRxData = NULL;
            }

            if( mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_PI )
            {
                if( mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_SRCADDR )
                {
                    pPhyStruct->flags |= gPhyFlagTxAckFP_c;
                }
                else
                {
                    pPhyStruct->flags &= ~gPhyFlagTxAckFP_c;
                }
            }
            Phy_GetRxParams();
            Radio_Phy_PdDataIndication(mPhyTaskInstance);

            break;

        case gCCA_c:
            if( (mStatusAndControlRegs[PHY_CTRL4] & (cPHY_CTRL4_CCATYPE << cPHY_CTRL4_CCATYPE_Shift_c)) == (gCcaED_c << cPHY_CTRL4_CCATYPE_Shift_c) )
            {
                /* ED */
#if gMWS_UseCoexistence_d
                MWS_CoexistenceReleaseAccess();
#endif
                Radio_Phy_PlmeEdConfirm(MCR20Drv_DirectAccessSPIRead((uint8_t) CCA1_ED_FNL), mPhyTaskInstance);
            }
            else
            {
                /* CCA */
                if( mStatusAndControlRegs[IRQSTS2] & cIRQSTS2_CCA )
                {
#if gMWS_UseCoexistence_d
                    MWS_CoexistenceReleaseAccess();
#endif
#if (gUseStandaloneCCABeforeTx_d == 1)
                    pPhyStruct->txParams.numOfCca = 0;
#endif
                    Radio_Phy_PlmeCcaConfirm(gPhyChannelBusy_c, mPhyTaskInstance);
                }
                else
                {
#if (gUseStandaloneCCABeforeTx_d == 1)
                    if( pPhyStruct->txParams.numOfCca > 0 )
                    {
                        uint8_t newState;

                        if( --pPhyStruct->txParams.numOfCca == 0 )
                        {
                            /* Perform TxRxAck sequence if required by phyTxMode */
                            if( gPhyRxAckRqd_c == pPhyStruct->txParams.ackRequired )
                            {
                                mStatusAndControlRegs[PHY_CTRL1] |= (uint8_t) (cPHY_CTRL1_RXACKRQD);
                                newState = gTR_c;
                            }
                            else
                            {
                                mStatusAndControlRegs[PHY_CTRL1] &= (uint8_t) ~(cPHY_CTRL1_RXACKRQD);
                                newState = gTX_c;
                            }
                        }
                        else
                        {
                            newState = gCCA_c;
                        }

                        mStatusAndControlRegs[PHY_CTRL1] &= (uint8_t) ~(cPHY_CTRL1_XCVSEQ);
                        mStatusAndControlRegs[PHY_CTRL1] |= newState;
                        
                        /* Unmask SEQ interrupt */
                        mStatusAndControlRegs[PHY_CTRL2] &= (uint8_t) ~(cPHY_CTRL2_SEQMSK);
                        /* Start the sequence immediately */
                        MCR20Drv_DirectAccessSPIMultiByteWrite(PHY_CTRL1,
                                                                 &mStatusAndControlRegs[PHY_CTRL1],
                                                                 2);
                        if( newState == gTR_c )
                        {
                            phyTime_t timeout;
                            macToPdDataMessage_t *pPD = (macToPdDataMessage_t*)pPhyStruct->pReq;

                            PhyTimeReadClock(&timeout);
                            timeout += gPhyWarmUpTime_c + gPhySHRDuration_c + 54 +
                                (pPD->msgData.dataReq.psduLength + 1) * gPhySymbolsPerOctet_c;
                            PhyTimeSetEventTimeout(&timeout);
                        }
                    }
                    else
#endif
                    {
#if gMWS_UseCoexistence_d
                        MWS_CoexistenceReleaseAccess();
#endif
                        Radio_Phy_PlmeCcaConfirm(gPhyChannelIdle_c, mPhyTaskInstance);
                    }
                }
            }
            break;

        case gCCCA_c:
#if gMWS_UseCoexistence_d
            MWS_CoexistenceReleaseAccess();
#endif
            Radio_Phy_PlmeCcaConfirm(gPhyChannelIdle_c, mPhyTaskInstance);
            break;

        default:
#if gMWS_UseCoexistence_d
            MWS_CoexistenceReleaseAccess();
#endif
            Radio_Phy_PlmeSyncLossIndication(mPhyTaskInstance);
            break;
        }
    }
    /* Timers interrupt */
    else
    {
        if( mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR2IRQ )
        {
            /* Disable TMR2 comparator and time triggered action */
            mStatusAndControlRegs[PHY_CTRL3]   &= (uint8_t) ~( cPHY_CTRL3_TMR2CMP_EN);
            mStatusAndControlRegs[PHY_CTRL1]   &= (uint8_t) ~( cPHY_CTRL1_TMRTRIGEN);

            MCR20Drv_DirectAccessSPIWrite(PHY_CTRL3, mStatusAndControlRegs[PHY_CTRL3]);
            MCR20Drv_DirectAccessSPIWrite(PHY_CTRL1, mStatusAndControlRegs[PHY_CTRL1]);

            Radio_Phy_TimeStartEventIndication(mPhyTaskInstance);
        }

        if( !(mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR3MSK) &&
             (mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR3IRQ) )
        {
            /* disable TMR3 comparator and timeout */
            mStatusAndControlRegs[PHY_CTRL3]   &= (uint8_t) ~( cPHY_CTRL3_TMR3CMP_EN);
            mStatusAndControlRegs[PHY_CTRL4]   &= (uint8_t) ~( cPHY_CTRL4_TC3TMOUT);

            MCR20Drv_DirectAccessSPIWrite(PHY_CTRL3, mStatusAndControlRegs[PHY_CTRL3]);
            MCR20Drv_DirectAccessSPIWrite(PHY_CTRL4, mStatusAndControlRegs[PHY_CTRL4]);

            /* Ensure that we're not issuing TimeoutIndication while the Automated sequence is still in progress */
            /* TMR3 can expire during R-T turnaround for example, case in which the sequence is not interrupted */
            if( gIdle_c == xcvseqCopy )
            {
                Radio_Phy_TimeRxTimeoutIndication(mPhyTaskInstance);
            }
        }

        /* Timer 4 Compare Match */
        if( !(mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR4MSK) &&
             (mStatusAndControlRegs[IRQSTS3] & cIRQSTS3_TMR4IRQ) )
        {
            /* disable TMR4 comparator */
            mStatusAndControlRegs[PHY_CTRL3]   &= (uint8_t) ~( cPHY_CTRL3_TMR4CMP_EN);
            MCR20Drv_DirectAccessSPIWrite(PHY_CTRL3, mStatusAndControlRegs[PHY_CTRL3]);
        }
    }

    Radio_Phy_Notify(mPhyTaskInstance);
}
#endif /* #if gPhyUseReducedSpiAccess_d */


/*! *********************************************************************************
* \brief  This function installs the PHY ISR
*
********************************************************************************** */
void PHY_InstallIsr( void )
{
    GpioInstallIsr(PHY_InterruptHandler, gGpioIsrPrioHigh_c, MCR20_Irq_Priority, &mXcvrIrqPinCfg);
}


/*! *********************************************************************************
* \brief  Fill the Rx parameters: RSSI, LQI, Timestamp and PSDU length
*
********************************************************************************** */
#if !gPhyUseReducedSpiAccess_d
static void Phy_GetRxParams(void)
{
    if(NULL != mpRxParams)
    {
        // reports value of 0x00 for -105 dBm of received input power and 0xFF for 0 dBm of received input power
        mPhyLastRxRSSI = MCR20Drv_DirectAccessSPIRead(LQI_VALUE);
        mpRxParams->linkQuality = Phy_LqiConvert(mPhyLastRxRSSI);
        mPhyLastRxLQI = mpRxParams->linkQuality;
        MCR20Drv_DirectAccessSPIMultiByteRead(TIMESTAMP_LSB, (uint8_t *)&mpRxParams->timeStamp, 3);
        mpRxParams->psduLength = mStatusAndControlRegs[RX_FRM_LEN]; //Including FCS (2 bytes)
    }
}
#endif

/*! *********************************************************************************
* \brief  Determines the state of the FP bit for an outgoing ACK frame.
*         If the source address is not prezent in the neighbor table, FP will be set.
*
* \param[in]  pPsdu  pointer to the PSDU
*
********************************************************************************** */
#if gPhyRxPBTransferThereshold_d
static void PreprocessPollReq(uint8_t *pPsdu)
{
#if gPhyNeighborTableSize_d
    uint8_t  dstAddrMode;
    uint8_t  srcAddrMode;
    bool_t   panIdCompression;
    uint16_t PanId;

    /* Get addressing information */
    dstAddrMode = (pPsdu[mFrameCtrlHi_d] & mDstAddrModeMask_d) >> mDstAddrModeShift_d;
    srcAddrMode = (pPsdu[mFrameCtrlHi_d] & mSrcAddrModeMask_d) >> mSrcAddrModeShift_d;
    panIdCompression = pPsdu[mFrameCtrlLo_d] & mPanIdCompression_d;
    
    /* Store dst PAN Id */
    pPsdu += mAddressingFields_d;
    PanId  = *pPsdu++;
    PanId |= (*pPsdu++) << 8;
    
    /* Skip over dst addr fields */
    if( dstAddrMode == mShortAddr_d )
    {
        pPsdu += sizeof(uint16_t);
    }
    else if( dstAddrMode == mExtAddr_d )
    {
        pPsdu += sizeof(uint64_t);
    }
    else if( dstAddrMode != mNoAddr_d )
    {
        mPhyForceFP = FALSE;
        return;
    }
    
    /* Store src PanId if present */
    if( !panIdCompression )
    {
        PanId  = *pPsdu++;
        PanId |= (*pPsdu++) << 8;
    }
    
    /* Get FP state */
    mPhyForceFP = !PhyCheckNeighborTable(PhyGetChecksum(pPsdu, srcAddrMode, PanId));

    if( mPhyForceFP )
    {
        PhyPpSetFpManually(TRUE);
    }
#endif
}
#endif

#if gPhyUseReducedSpiAccess_d
static void Phy_RestartRxFromISR()
{
    /* Restart RX ASAP, once a sequence ends */
    if( (phyLocal[mPhyTaskInstance].flags & gPhyFlagRxOnWhenIdle_c) &&
       !MSG_Pending(&phyLocal[mPhyTaskInstance].macPhyInputQueue) 
#if gMpmIncluded_d
       /* Prepare the Active PAN/PANs */
       && (gPhySuccess_c == MPM_PrepareForRx(gInvalidInstanceId_c))
#endif
      )
    {
        phyTime_t time;

        phyLocal[mPhyTaskInstance].flags |= gPhyFlagIdleRx_c;
        phyLocal[mPhyTaskInstance].rxParams.timeStamp = gPhySeqStartAsap_c;
        phyLocal[mPhyTaskInstance].rxParams.duration  = gPhyMaxIdleRxDuration_c;
        phyLocal[mPhyTaskInstance].rxParams.phyRxMode = gPhyUnslottedMode_c;
        PhyPlmeRxRequest( gPhyUnslottedMode_c, &phyLocal[mPhyTaskInstance].rxParams );

        PhyTimeReadClock(&time);
        time += gPhyMaxIdleRxDuration_c;
        PhyTimeSetEventTimeout(&time);
    }
}
#endif

/*! *********************************************************************************
* \brief  Performs RF abort sequence and notify the upper layer
*
********************************************************************************** */
#if gMWS_UseCoexistence_d
static void Phy_CoexAbort(Phy_PhyLocalStruct_t *pPhyStruct)
{
    uint8_t xcvrState = PhyGetSeqState();
    
    PhyAbort();
    mCoexAbortPending = 0;
    
    if( pPhyStruct->PLME_MAC_SapHandler )
    {
        switch(xcvrState)
        {
        case gCCA_c:
            if( pPhyStruct->pReq )
            {
                Radio_Phy_PlmeEdConfirm(MCR20Drv_DirectAccessSPIRead(CCA1_ED_FNL), mPhyTaskInstance);
                break;
            }
        case gTX_c:
        case gTR_c:
            Radio_Phy_PlmeCcaConfirm(gPhyChannelBusy_c, mPhyTaskInstance);
            break;
        case gRX_c:
            break;
        default:
            Radio_Phy_TimeRxTimeoutIndication(mPhyTaskInstance);
            break;
        }
    }
}
#endif