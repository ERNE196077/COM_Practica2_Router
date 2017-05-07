/*!
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* All rights reserved.
*
* \file ASP.c
* This is the source file for the ASP module.
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

#include "Phy.h"
#include "PhyInterface.h"
#include "MpmInterface.h"
#include "AspInterface.h"
#include "MemManager.h"
#include "FunctionLib.h"

#include "MCR20Drv.h"
#include "MCR20Reg.h"

#if gFsciIncluded_c
#include "FsciInterface.h"
#include "FsciAspCommands.h"
#endif

#ifdef gSmacSupported
#include "SMAC_Interface.h"
#endif

#if gAspCapability_d

/*! *********************************************************************************
*************************************************************************************
* Public macros
*************************************************************************************
********************************************************************************** */
#define mFAD_THR_ResetValue         0x82
#define mANT_AGC_CTRL_ResetValue    0x40
#define mASP_MinTxIntervalMS_d      (5)


/*! *********************************************************************************
*************************************************************************************
* Public type definitions
*************************************************************************************
********************************************************************************** */
/* MCR20 DTS modes */
enum {
  gDtsNormal_c,
  gDtsTxOne_c,
  gDtsTxZero_c,
  gDtsTx2Mhz_c,
  gDtsTx200Khz_c,
  gDtsTx1MbpsPRBS9_c,
  gDtsTxExternalSrc_c,
  gDtsTxRandomSeq_c
};


/*! *********************************************************************************
*************************************************************************************
* Private functions prototype
*************************************************************************************
********************************************************************************** */
phyStatus_t AspSetDtsMode( uint8_t mode );
phyStatus_t AspEnableBER( void );
void AspDisableBER( void );
static void ASP_TxInterval ( uint32_t param );

/*! *********************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
********************************************************************************** */
                                        /* 2405    2410    2415    2420    2425    2430    2435    2440    2445    2450    2455    2460    2465    2470    2475    2480 */
static const uint16_t asp_pll_frac[16] = {0x2400, 0x4C00, 0x7400, 0x9C00, 0xC400, 0xEC00, 0x1400, 0x3C00, 0x6400, 0x8C00, 0xB400, 0xDC00, 0x0400, 0x2C00, 0x5400, 0x7C00};

static uint32_t mAsp_TxIntervalMs = mASP_MinTxIntervalMS_d;
static phyTimeTimerId_t mAsp_TxTimer = gInvalidTimerId_c;
static const uint8_t mAsp_Prbs9Packet[65] =
{
    0x42,
    0xff,0xc1,0xfb,0xe8,0x4c,0x90,0x72,0x8b,0xe7,0xb3,0x51,0x89,0x63,0xab,0x23,0x23,  
    0x02,0x84,0x18,0x72,0xaa,0x61,0x2f,0x3b,0x51,0xa8,0xe5,0x37,0x49,0xfb,0xc9,0xca,
    0x0c,0x18,0x53,0x2c,0xfd,0x45,0xe3,0x9a,0xe6,0xf1,0x5d,0xb0,0xb6,0x1b,0xb4,0xbe,
    0x2a,0x50,0xea,0xe9,0x0e,0x9c,0x4b,0x5e,0x57,0x24,0xcc,0xa1,0xb7,0x59,0xb8,0x87
};

/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
* \brief  Initialize the ASP module
*
* \param[in]  phyInstance The instance of the PHY
* \param[in]  interfaceId The FSCI interface used
*
********************************************************************************** */
void ASP_Init( instanceId_t phyInstance )
{
}

/*! *********************************************************************************
* \brief  ASP SAP handler.
*
* \param[in]  pMsg        Pointer to the request message
* \param[in]  instanceId  The instance of the PHY
*
* \return  AspStatus_t
*
********************************************************************************** */
AspStatus_t APP_ASP_SapHandler(AppToAspMessage_t *pMsg, instanceId_t phyInstance)
{
    AspStatus_t status = gAspSuccess_c;
#if gFsciIncluded_c
    FSCI_Monitor( gFSCI_AspSapId_c,
                  pMsg,
                  NULL,
                  fsciGetAspInterfaceId(phyInstance) );
#endif
    switch( pMsg->msgType )
    {
    case aspMsgTypeGetTimeReq_c:
        Asp_GetTimeReq((uint64_t*)&pMsg->msgData.aspGetTimeReq.time);
        break;
    case aspMsgTypeXcvrWriteReq_c:
        status = Asp_XcvrWriteReq( pMsg->msgData.aspXcvrData.mode,
                                   pMsg->msgData.aspXcvrData.addr,
                                   pMsg->msgData.aspXcvrData.len,
                                   pMsg->msgData.aspXcvrData.data);
        break;
    case aspMsgTypeXcvrReadReq_c:
        status = Asp_XcvrReadReq( pMsg->msgData.aspXcvrData.mode,
                                  pMsg->msgData.aspXcvrData.addr,
                                  pMsg->msgData.aspXcvrData.len,
                                  pMsg->msgData.aspXcvrData.data);
        break;
    case aspMsgTypeSetFADState_c:
        status = Asp_SetFADState(pMsg->msgData.aspFADState);
        break;
    case aspMsgTypeSetFADThreshold_c:
        status = Asp_SetFADThreshold(pMsg->msgData.aspFADThreshold);
        break;
    case aspMsgTypeSetANTXState_c:
        status = Asp_SetANTXState(pMsg->msgData.aspANTXState);
        break;
    case aspMsgTypeGetANTXState_c:
        *((uint8_t*)&status) = Asp_GetANTXState();
        break;
    case aspMsgTypeSetPowerLevel_c:
        status = Asp_SetPowerLevel(pMsg->msgData.aspSetPowerLevelReq.powerLevel);
        break;
    case aspMsgTypeGetPowerLevel_c:
        *((uint8_t*)&status) = Asp_GetPowerLevel(); /* remove compiler warning */
        break;
    case aspMsgTypeTelecSetFreq_c:
        status = ASP_TelecSetFreq(pMsg->msgData.aspTelecsetFreq.channel);
        break;
    case aspMsgTypeTelecSendRawData_c:
        status = ASP_TelecSendRawData((uint8_t*)&pMsg->msgData.aspTelecSendRawData);
        break;
    case aspMsgTypeTelecTest_c:
        status = ASP_TelecTest(pMsg->msgData.aspTelecTest.mode);
        break;
    case aspMsgTypeSetLQIMode_c:
        status = Asp_SetLQIMode(pMsg->msgData.aspLQIMode);
        break;
    case aspMsgTypeGetRSSILevel_c:
        *((uint8_t*)&status) = Asp_GetRSSILevel(); /* remove compiler warning */
        break;
    case aspMsgTypeSetTxInterval_c:
        if( pMsg->msgData.aspSetTxInterval.intervalMs >= mASP_MinTxIntervalMS_d )
        {
            mAsp_TxIntervalMs = pMsg->msgData.aspSetTxInterval.intervalMs;
        }
        else
        {
            status = gAspInvalidParameter_c;
        }
        break;
#if gMpmIncluded_d
    case aspMsgTypeSetMpmConfig_c:
        {
            mpmConfig_t cfg = {
                .autoMode = pMsg->msgData.MpmConfig.autoMode,
                .dwellTime = pMsg->msgData.MpmConfig.dwellTime,
                .activeMAC = pMsg->msgData.MpmConfig.activeMAC
            };

            MPM_SetConfig(&cfg);
        }
        break;
    case aspMsgTypeGetMpmConfig_c:
        {
            mpmConfig_t cfg;

            MPM_GetConfig(&cfg);
            pMsg->msgData.MpmConfig.autoMode = cfg.autoMode;
            pMsg->msgData.MpmConfig.dwellTime = cfg.dwellTime;
            pMsg->msgData.MpmConfig.activeMAC = cfg.activeMAC;
        }
        break;
#endif
    default:
        status = gAspInvalidRequest_c; /* OR gAspInvalidParameter_c */
        break;
    }
#if gFsciIncluded_c
    FSCI_Monitor( gFSCI_AspSapId_c,
                  pMsg,
                  (void*)&status,
                  fsciGetAspInterfaceId(phyInstance) );
#endif
    return status;
}

/*! *********************************************************************************
* \brief  Returns the current PHY time
*
* \param[in]  time  location where the PHY time will be stored
*
********************************************************************************** */
void Asp_GetTimeReq(uint64_t *time)
{
    PhyTimeReadClock( time );
}

/*! *********************************************************************************
* \brief  Write XCVR registers
*
* \param[in]  mode   Direct/Indirect access
* \param[in]  addr   address
* \param[in]  len    number of bytes to write
* \param[in]  pData  data o be written
*
* \return  AspStatus_t
*
********************************************************************************** */
AspStatus_t Asp_XcvrWriteReq (uint8_t mode, uint16_t addr, uint8_t len, uint8_t* pData)
{
    if (mode)
    {
        MCR20Drv_IndirectAccessSPIMultiByteWrite((uint8_t)addr, pData, len);
    }
    else
    {
        MCR20Drv_DirectAccessSPIMultiByteWrite((uint8_t)addr, pData, len);
    }

    return gAspSuccess_c;
}

/*! *********************************************************************************
* \brief  Read XCVR registers
*
* \param[in]  mode   Direct/Indirect access
* \param[in]  addr   XCVR address
* \param[in]  len    number of bytes to read
* \param[in]  pData  location where data will be stored
*
* \return  AspStatus_t
*
********************************************************************************** */
AspStatus_t Asp_XcvrReadReq  (uint8_t mode, uint16_t addr, uint8_t len, uint8_t* pData)
{
    if (mode)
    {
        MCR20Drv_IndirectAccessSPIMultiByteRead((uint8_t)addr, pData, len);
    }
    else
    {
        MCR20Drv_DirectAccessSPIMultiByteRead((uint8_t)addr, pData, len);
    }

    return gAspSuccess_c;
}

/*! *********************************************************************************
* \brief  Set Tx output power level
*
* \param[in]  powerLevel   The new power level: 0x03-0x1F (see documentation for details)
*
* \return  AspStatus_t
*
********************************************************************************** */
AspStatus_t Asp_SetPowerLevel( uint8_t powerLevel )
{
    AspStatus_t res = gAspSuccess_c;

    if( gPhySuccess_c != PhyPlmeSetPwrLevelRequest(powerLevel) )
    {
        res = gAspInvalidParameter_c;
    }

    return res;
}

/*! *********************************************************************************
* \brief  Read the current Tx power level
*
* \return  power level
*
********************************************************************************** */
uint8_t Asp_GetPowerLevel(void)
{
    return MCR20Drv_DirectAccessSPIRead(PA_PWR);
}

/*! *********************************************************************************
* \brief  Set the state of Active Promiscuous functionality
*
* \param[in]  state  new state 
*
* \return  AspStatus_t
*
********************************************************************************** */
AspStatus_t Asp_SetActivePromState(bool_t state)
{
    PhySetActivePromiscuous(state);
    return gAspSuccess_c;
}

/*! *********************************************************************************
* \brief  Set the state of Fast Antenna Diversity functionality
*
* \param[in]  state  new state 
*
* \return  AspStatus_t
*
********************************************************************************** */
AspStatus_t Asp_SetFADState(bool_t state)
{
    AspStatus_t status = gAspSuccess_c;

    if( gPhySuccess_c != PhyPlmeSetFADStateRequest(state) )
    {
        status = gAspDenied_c;
    }

    return status;
}

/*! *********************************************************************************
* \brief  Set the Fast Antenna Diversity threshold
*
* \param[in]  threshold 
*
* \return  AspStatus_t
*
********************************************************************************** */
AspStatus_t Asp_SetFADThreshold(uint8_t thresholdFAD)
{
    AspStatus_t status = gAspSuccess_c;

    if( gPhySuccess_c != PhyPlmeSetFADThresholdRequest(thresholdFAD) )
    {
        status = gAspDenied_c;
    }
    return status;
}

/*! *********************************************************************************
* \brief  Set the ANTX functionality
*
* \param[in]  state 
*
* \return  AspStatus_t
*
********************************************************************************** */
AspStatus_t Asp_SetANTXState(bool_t state)
{
    AspStatus_t status = gAspSuccess_c;
    
    if( gPhySuccess_c != PhyPlmeSetANTXStateRequest(state) )
    {
        status = gAspDenied_c;
    }
    return status;
}

/*! *********************************************************************************
* \brief  Get the ANTX functionality
*
* \return  current state
*
********************************************************************************** */
uint8_t Asp_GetANTXState(void)
{
  return PhyPlmeGetANTXStateRequest();
}

/*! *********************************************************************************
* \brief  Set the ANTX pad state
*
* \param[in]  antAB_on 
* \param[in]  rxtxSwitch_on 
*
* \return  status
*
********************************************************************************** */
uint8_t Asp_SetANTPadStateRequest(bool_t antAB_on, bool_t rxtxSwitch_on)
{
    return PhyPlmeSetANTPadStateRequest(antAB_on, rxtxSwitch_on);
}

/*! *********************************************************************************
* \brief  Set the ANTX pad strength
*
* \param[in]  hiStrength 
*
* \return  status
*
********************************************************************************** */
uint8_t Asp_SetANTPadStrengthRequest(bool_t hiStrength)
{
    return PhyPlmeSetANTPadStrengthRequest(hiStrength);
}

/*! *********************************************************************************
* \brief  Set the ANTX inverted pads
*
* \param[in]  invAntA  invert Ant_A pad
* \param[in]  invAntB  invert Ant_B pad
* \param[in]  invTx    invert Tx pad
* \param[in]  invRx    invert Rx pad
*
* \return  status
*
********************************************************************************** */
uint8_t Asp_SetANTPadInvertedRequest(bool_t invAntA, bool_t invAntB, bool_t invTx, bool_t invRx)
{
    return PhyPlmeSetANTPadInvertedRequest(invAntA, invAntB, invTx, invRx);
}

/*! *********************************************************************************
* \brief  Set the LQI mode
*
* \param[in]  mode 
*
* \return  AspStatus_t
*
********************************************************************************** */
AspStatus_t Asp_SetLQIMode(bool_t mode)
{
    AspStatus_t status = gAspSuccess_c;

    if( gPhySuccess_c != PhyPlmeSetLQIModeRequest(mode) )
    {
        status = gAspDenied_c;
    }
    return status;
}

/*! *********************************************************************************
* \brief  Get the last RSSI level
*
* \return  RSSI
*
********************************************************************************** */
uint8_t Asp_GetRSSILevel(void)
{
  return PhyPlmeGetRSSILevelRequest();
}

/*! *********************************************************************************
* \brief  Set current channel
*
* \param[in]  channel  channel number (11-26)
*
* \return  AspStatus_t
*
********************************************************************************** */
AspStatus_t ASP_TelecSetFreq(uint8_t channel)
{
    AspStatus_t status = gAspSuccess_c;
    
    PhyPlmeForceTrxOffRequest();
    
    if( gPhySuccess_c != PhyPlmeSetCurrentChannelRequest(channel,0) )
    {
        status = gAspInvalidParameter_c;
    }

    return status;
}

/*! *********************************************************************************
* \brief  Send a raw data frame OTA
*
* \param[in]  dataPtr  raw data
*
* \return  AspStatus_t
*
********************************************************************************** */
AspStatus_t ASP_TelecSendRawData(uint8_t* dataPtr)
{
    uint8_t temp = dataPtr[0] + 2;

    /* Validate the length */
    if(temp > gMaxPHYPacketSize_c)
        return gAspTooLong_c;

    /* Force Idle */
    PhyPlmeForceTrxOffRequest();
    AspSetDtsMode(gDtsNormal_c);
    AspDisableBER();
    /* Load the TX PB: load the PSDU Lenght byte but not the FCS bytes */
    MCR20Drv_PB_SPIBurstWrite(dataPtr, dataPtr[0] + 1);
    MCR20Drv_PB_SPIBurstWrite(&temp, 1);
    /* Program a Tx sequence */
    temp = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
    temp |=  gTX_c;
    MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, temp);
    return gAspSuccess_c;
}

/*! *********************************************************************************
* \brief  Set Telec test mode
*
* \param[in]  mode  Telec test mode
*
* \return  AspStatus_t
*
********************************************************************************** */
AspStatus_t ASP_TelecTest(uint8_t mode)
{
    uint8_t phyReg;
    static uint8_t aTxContModPattern[2];
    AspStatus_t status = gAspSuccess_c;
    uint8_t channel;
    static bool_t fracSet = FALSE;

    /* Get current channel number */
    channel = PhyPlmeGetCurrentChannelRequest(0);

    if( fracSet )
    {
        ASP_TelecSetFreq(channel);
        fracSet = FALSE;
    }

    switch( mode )
    {
    case gTestForceIdle_c:  /* ForceIdle() */
        /* Stop Tx interval timer (if started) */
        PhyTime_CancelEvent(mAsp_TxTimer);
#ifdef gSmacSupported
        MLMEPhySoftReset();
#else
        PhyPlmeForceTrxOffRequest();
#endif
        AspSetDtsMode(gDtsNormal_c);
        AspDisableBER();
        break;

    case gTestPulseTxPrbs9_c:   /* Continuously transmit a PRBS9 pattern. */
        // PLME_PRBS9_Load (); /* Load the TX RAM */
        AspSetDtsMode(gDtsTxRandomSeq_c);
        /* Enable continuous TX mode */
        AspEnableBER();
        /* Start Tx packet mode with no interrupt on end */
        phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
        phyReg |=  gTX_c;
        MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, phyReg);
        break;

    case gTestContinuousRx_c: /* Sets the device into continuous RX mode */
        AspSetDtsMode(gDtsNormal_c);
        /* Enable continuous RX mode */
        AspEnableBER();
        /* Set length of data in DUAL_PAN_DWELL register */
        MCR20Drv_IndirectAccessSPIWrite(DUAL_PAN_DWELL, 127);
        /* Start Rx packet mode with no interrupt on end */
        phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
        phyReg |=  gRX_c;
        MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, phyReg);
        break;

    case gTestContinuousTxMod_c: /* Sets the device to continuously transmit a 10101010 pattern */
        AspSetDtsMode(gDtsNormal_c);
        /* Enable continuous TX mode */
        AspEnableBER();
        /* Prepare TX operation */
        aTxContModPattern[0] = 1;
        aTxContModPattern[1] = 0xAA;
        /* Load the TX PB */
        MCR20Drv_PB_SPIBurstWrite(aTxContModPattern, aTxContModPattern[0] + 1);
        /* Program a Tx sequence */
        phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
        phyReg |=  gTX_c;
        MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, phyReg);
        break;

    case gTestContinuousTxNoMod_c: /* Sets the device to continuously transmit an unmodulated CW */
        /* Enable unmodulated TX */
        AspSetDtsMode(gDtsTxOne_c);
        /* Enable continuous TX mode */
        AspEnableBER();
        MCR20Drv_DirectAccessSPIMultiByteWrite(PLL_FRAC0_LSB, (uint8_t *) &asp_pll_frac[channel - 11], 2);
        fracSet = TRUE;
        /* Program a Tx sequence */
        phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
        phyReg |=  gTX_c;
        MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, phyReg);
        break;

    case gTestContinuousTx2Mhz_c:
        AspSetDtsMode(gDtsTx2Mhz_c);
        /* Enable continuous TX mode */
        AspEnableBER();
        /* Program a Tx sequence */
        phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
        phyReg |=  gTX_c;
        MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, phyReg);
        break;

    case gTestContinuousTx200Khz_c:
        AspSetDtsMode(gDtsTx200Khz_c);
        /* Enable continuous TX mode */
        AspEnableBER();
        /* Program a Tx sequence */
        phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
        phyReg |=  gTX_c;
        MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, phyReg);
        break;

    case gTestContinuousTx1MbpsPRBS9_c:
        AspSetDtsMode(gDtsTx1MbpsPRBS9_c);
        /* Enable continuous TX mode */
        AspEnableBER();
        /* Program a Tx sequence */
        phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
        phyReg |=  gTX_c;
        MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, phyReg);
        break;

    case gTestContinuousTxExternalSrc_c:
        AspSetDtsMode(gDtsTxExternalSrc_c);
        /* Enable continuous TX mode */
        AspEnableBER();
        /* Program a Tx sequence */
        phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
        phyReg |=  gTX_c;
        MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, phyReg);
        break;

    case gTestContinuousTxModZero_c:
        /* Enable unmodulated TX */
        AspSetDtsMode(gDtsTxZero_c);
        /* Enable continuous TX mode */
        AspEnableBER();
        /* Program a Tx sequence */
        phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
        phyReg |=  gTX_c;
        MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, phyReg);
        break;

    case gTestContinuousTxModOne_c:
        /* Enable unmodulated TX */
        AspSetDtsMode(gDtsTxOne_c);
        /* Enable continuous TX mode */
        AspEnableBER();
        /* Program a Tx sequence */
        phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL1);
        phyReg |=  gTX_c;
        MCR20Drv_DirectAccessSPIWrite( (uint8_t) PHY_CTRL1, phyReg);
        break;

    case gTestTxPacketPRBS9_c:
        ASP_TxInterval( (uint32_t)mAsp_Prbs9Packet );
        break;

    default:
        status = gAspInvalidParameter_c;
        break;
    }

    return status;
}

/*! *********************************************************************************
* \brief  Transmit a raw data packet at a specific interval
*
* \param[in]  address of the raw data packet
*
********************************************************************************** */
static void ASP_TxInterval ( uint32_t param )
{
    phyTimeEvent_t ev;
    
    /* convert interval to symbols */
    ev.timestamp = (mAsp_TxIntervalMs * 1000) / 16;
    ev.timestamp += PhyTime_GetTimestamp();
    ev.callback = ASP_TxInterval;
    ev.parameter = param;
    mAsp_TxTimer = PhyTime_ScheduleEvent(&ev);
    
    ASP_TelecSendRawData( (uint8_t*)param );
}

 /*! *********************************************************************************
* \brief  Set the Tx data source selector
*
* \param[in]  mode 
*
* \return  AspStatus_t
*
********************************************************************************** */
phyStatus_t AspSetDtsMode(uint8_t mode)
{
  uint8_t phyReg;

  phyReg = MCR20Drv_IndirectAccessSPIRead(TX_MODE_CTRL);
  phyReg &= ~cTX_MODE_CTRL_DTS_MASK;   /* Clear DTS_MODE */
  phyReg |= mode; /* Set new DTS_MODE */
  MCR20Drv_IndirectAccessSPIWrite(TX_MODE_CTRL, phyReg);

  return gPhySuccess_c;
}

/*! *********************************************************************************
* \brief  Enable XCVR test mode
*
* \return  AspStatus_t
*
********************************************************************************** */
phyStatus_t AspEnableBER()
{
  uint8_t phyReg;

  phyReg = MCR20Drv_IndirectAccessSPIRead(DTM_CTRL1);
  phyReg |= cDTM_CTRL1_DTM_EN;
  MCR20Drv_IndirectAccessSPIWrite(DTM_CTRL1, phyReg);

  phyReg = MCR20Drv_IndirectAccessSPIRead(TESTMODE_CTRL);
  phyReg |= cTEST_MODE_CTRL_CONTINUOUS_EN | cTEST_MODE_CTRL_IDEAL_PFC_EN;
  MCR20Drv_IndirectAccessSPIWrite(TESTMODE_CTRL, phyReg);

  return gPhySuccess_c;
}

/*! *********************************************************************************
* \brief  Disable XCVR test mode
*
********************************************************************************** */
void AspDisableBER()
{
  uint8_t phyReg;

  phyReg = MCR20Drv_IndirectAccessSPIRead(DTM_CTRL1);
  phyReg &= ~cDTM_CTRL1_DTM_EN;
  MCR20Drv_IndirectAccessSPIWrite(DTM_CTRL1, phyReg);

  phyReg = MCR20Drv_IndirectAccessSPIRead(TESTMODE_CTRL);
  phyReg &= ~(cTEST_MODE_CTRL_CONTINUOUS_EN | cTEST_MODE_CTRL_IDEAL_PFC_EN);
  MCR20Drv_IndirectAccessSPIWrite(TESTMODE_CTRL, phyReg);
}


#endif /* gAspCapability_d */