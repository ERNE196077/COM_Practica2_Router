/*!
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* All rights reserved.
*
* \file
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

#include "PhyInterface.h"
#include "Phy.h"
#include "MemManager.h"
#include "Messaging.h"
#include "FunctionLib.h"

#include "MCR20Drv.h"
#include "MCR20Reg.h"

#include "AspInterface.h"
#include "MpmInterface.h"

#ifndef gMWS_UseCoexistence_d
#define gMWS_UseCoexistence_d 0
#endif

#if gMWS_UseCoexistence_d
#include "MWS.h"
#include "gpio_pins.h"
#endif


/*! *********************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
********************************************************************************** */


/*! *********************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
*************************************************************************************
* Private prototypes
*************************************************************************************
********************************************************************************** */
static void Phy24Task(Phy_PhyLocalStruct_t *pPhyData);

static phyStatus_t Phy_HandlePdDataReq( Phy_PhyLocalStruct_t *pPhyData, macToPdDataMessage_t * pMsg );

static void Phy_EnterIdle( Phy_PhyLocalStruct_t *pPhyData );

static void PLME_SendMessage(Phy_PhyLocalStruct_t *pPhyData, phyMessageId_t msgType);

static void PD_SendMessage(Phy_PhyLocalStruct_t *pPhyData, phyMessageId_t msgType);

static void Phy_SendLatePD( uint32_t param );
static void Phy_SendLatePLME( uint32_t param );
#if gMWS_UseCoexistence_d
static uint32_t MWS_802_15_4_Callback ( mwsEvents_t event );
#endif


/*! *********************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
********************************************************************************** */
Phy_PhyLocalStruct_t phyLocal[gPhyInstancesCnt_c];
extern volatile uint32_t mPhySeqTimeout;
uint8_t mXcvrDisallowSleep = 0;
const uint8_t gPhyPoolId = gPhyPoolId_d;
#if gMWS_UseCoexistence_d
uint8_t mCoexAbortPending = 0;
#endif


/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

/*! *********************************************************************************
* \brief  This function creates the PHY task
*
********************************************************************************** */
void Phy_Init(void)
{
    uint32_t i;

    PhyHwInit();
    PhyTime_TimerInit(NULL);
    ASP_Init(0);
    MPM_Init();

    for( i=0; i<gPhyInstancesCnt_c; i++ )
    {
        phyLocal[i].flags = gPhyFlagDeferTx_c;
        phyLocal[i].rxParams.pRxData = NULL;
        phyLocal[i].rxParams.pDataPending = NULL;
        phyLocal[i].pReq = NULL;

        /* Prepare input queues.*/
        MSG_InitQueue( &phyLocal[i].macPhyInputQueue );
    }

    PhyIsrPassRxParams( NULL );
    PhyPlmeSetPwrState( gPhyDefaultIdlePwrMode_c );
#if gMWS_UseCoexistence_d
    MWS_CoexistenceInit(&gRf_Confirm, &gRf_Active, &gRf_Status);
    MWS_CoexistenceRegister(gMWS_802_15_4_c, MWS_802_15_4_Callback);
#endif
}

/*! *********************************************************************************
* \brief  This function binds a MAC instance to a PHY instance
*
* \param[in]  instanceId The instance of the MAC
*
* \return  The instance of the PHY.
*
********************************************************************************** */
instanceId_t BindToPHY( instanceId_t macInstance )
{
    return 0;
}

/*! *********************************************************************************
* \brief  This function registers the MAC PD and PLME SAP handlers
*
* \param[in]  pPD_MAC_SapHandler   Pointer to the MAC PD handler function
* \param[in]  pPLME_MAC_SapHandler Pointer to the MAC PLME handler function
* \param[in]  instanceId           The instance of the PHY
*
* \return  The status of the operation.
*
********************************************************************************** */
void Phy_RegisterSapHandlers( PD_MAC_SapHandler_t pPD_MAC_SapHandler,
                              PLME_MAC_SapHandler_t pPLME_MAC_SapHandler,
                              instanceId_t instanceId )
{
    phyLocal[instanceId].PD_MAC_SapHandler = pPD_MAC_SapHandler;
    phyLocal[instanceId].PLME_MAC_SapHandler = pPLME_MAC_SapHandler;
}

/*! *********************************************************************************
* \brief  This function represents the PHY's task
*
* \param[in]  taskParam The instance of the PHY
*
********************************************************************************** */
static void Phy24Task(Phy_PhyLocalStruct_t *pPhyStruct)
{
    uint8_t state;
    phyMessageHeader_t * pMsgIn = NULL;
    phyStatus_t status;

    ProtectFromXcvrInterrupt();

    /* Handling messages from upper layer */
    while( MSG_Pending(&pPhyStruct->macPhyInputQueue) )
    {
        status = gPhySuccess_c;
        state = PhyGetSeqState();
        /* PHY doesn't free dynamic alocated messages! */
        pMsgIn = MSG_DeQueue( &pPhyStruct->macPhyInputQueue );

        if( gRX_c == state )
        {
            if( (pPhyStruct->flags & gPhyFlagDeferTx_c) && (pMsgIn->msgType == gPdDataReq_c) )
            {
                macToPdDataMessage_t *pPD = (macToPdDataMessage_t*)pMsgIn;
                uint8_t phyReg = MCR20Drv_DirectAccessSPIRead(SEQ_STATE) & 0x1F;

                /* Check for an Rx in progress. */
                if( (pPD->msgData.dataReq.CCABeforeTx != gPhyNoCCABeforeTx_c) && 
                    (pPD->msgData.dataReq.startTime == gPhySeqStartAsap_c) &&
                    (pPD->msgData.dataReq.slottedTx == gPhyUnslottedMode_c) &&
                    ((phyReg <= 0x06) || (phyReg == 0x15) || (phyReg == 0x16)) )
                {
                    /* Postpone TX until the Rx has finished */
                    pPhyStruct->flags |= gPhyFlaqReqPostponed_c;
                    MSG_QueueHead( &pPhyStruct->macPhyInputQueue, pMsgIn );
                    break;
                }
            }

            //if( pPhyStruct->flags & gPhyFlagIdleRx_c )
            {
                PhyPlmeForceTrxOffRequest();
                state = gIdle_c;
                pPhyStruct->flags &= ~(gPhyFlagIdleRx_c);
            }
        }

        if( gIdle_c != state )
        {
            /* Try again later */
            MSG_QueueHead( &pPhyStruct->macPhyInputQueue, pMsgIn );
            break;
        }

        pPhyStruct->currentMacInstance = pMsgIn->macInstance;
        pPhyStruct->flags &= ~(gPhyFlaqReqPostponed_c);

#if gMpmIncluded_d
        if( status == gPhySuccess_c )
        {
            status = MPM_PrepareForTx( pMsgIn->macInstance );
        }
#endif

        if( status == gPhySuccess_c )
        {
            pPhyStruct->flags &= ~(gPhyFlagIdleRx_c);
            pPhyStruct->pReq = pMsgIn;

            switch( pMsgIn->msgType )
            {
            case gPdDataReq_c:
                status = Phy_HandlePdDataReq( pPhyStruct, (macToPdDataMessage_t *)pMsgIn );
                break;
            case gPlmeCcaReq_c:
                status = PhyPlmeCcaEdRequest(gPhyCCAMode1_c, gPhyContCcaDisabled);
                break;
            case gPlmeEdReq_c:
                status = PhyPlmeCcaEdRequest(gPhyEnergyDetectMode_c, gPhyContCcaDisabled);
                break;
            default:
                status = gPhyInvalidPrimitive_c;
                break;
            }
        }

        /* Check status */
        if( gPhySuccess_c != status )
        {
            switch( pMsgIn->msgType )
            {
            case gPdDataReq_c:
//                PD_SendMessage(pPhyStruct, gPdDataCnf_c);
//                break;
                /* Fallthorough */
            case gPlmeCcaReq_c:
                pPhyStruct->channelParams.channelStatus = gPhyChannelBusy_c;
                PLME_SendMessage(pPhyStruct, gPlmeCcaCnf_c);
                break;
            case gPlmeEdReq_c:
                pPhyStruct->channelParams.energyLeveldB = 0;
                PLME_SendMessage(pPhyStruct, gPlmeEdCnf_c);
                break;
            default:
                PLME_SendMessage(pPhyStruct, gPlmeTimeoutInd_c);
                break;
            }
        }
    }/* while( MSG_Pending(&pPhyStruct->macPhyInputQueue) ) */

    UnprotectFromXcvrInterrupt();

    /* Check if PHY can enter Idle state */
    if( gIdle_c == PhyGetSeqState() )
    {
        Phy_EnterIdle( pPhyStruct );
    }
}

/*! *********************************************************************************
* \brief  This is the PD SAP message handler
*
* \param[in]  pMsg Pointer to the PD request message
* \param[in]  instanceId The instance of the PHY
*
* \return  The status of the operation.
*
********************************************************************************** */
phyStatus_t MAC_PD_SapHandler(macToPdDataMessage_t *pMsg, instanceId_t phyInstance)
{
    phyStatus_t result = gPhySuccess_c;
    uint8_t baseIndex = 0;

    if( NULL == pMsg )
    {
        return gPhyInvalidParameter_c;
    }

#if gMpmIncluded_d
    if( pMsg->msgType == gPdIndQueueInsertReq_c || pMsg->msgType == gPdIndQueueRemoveReq_c )
    {
        baseIndex = MPM_GetRegSet( MPM_GetPanIndex( pMsg->macInstance ) ) *
                   (gPhyIndirectQueueSize_c/gMpmPhyPanRegSets_c);
    }
#endif

    switch( pMsg->msgType )
    {
    case gPdIndQueueInsertReq_c:
        result = PhyPp_IndirectQueueInsert(baseIndex + pMsg->msgData.indQueueInsertReq.index,
                                           pMsg->msgData.indQueueInsertReq.checksum,
                                           phyInstance);
        break;

    case gPdIndQueueRemoveReq_c:
        result = PhyPp_RemoveFromIndirect(baseIndex + pMsg->msgData.indQueueRemoveReq.index,
                                          phyInstance);
        break;

    case gPdDataReq_c:
        MSG_Queue(&phyLocal[phyInstance].macPhyInputQueue, pMsg);
        Phy24Task( &phyLocal[phyInstance] );
        break;

    default:
        result = gPhyInvalidPrimitive_c;
        break;
    }

    return result;
}

/*! *********************************************************************************
* \brief  This is the PLME SAP message handler
*
* \param[in]  pMsg Pointer to the PLME request message
* \param[in]  instanceId The instance of the PHY
*
* \return  phyStatus_t The status of the operation.
*
********************************************************************************** */
phyStatus_t MAC_PLME_SapHandler(macToPlmeMessage_t * pMsg, instanceId_t phyInstance)
{
    Phy_PhyLocalStruct_t *pPhyStruct = &phyLocal[phyInstance];
    uint8_t phyRegSet = 0;
#if gMpmIncluded_d
    phyStatus_t result;
    int32_t panIdx = MPM_GetPanIndex( pMsg->macInstance );

    phyRegSet = MPM_GetRegSet( panIdx );
#endif

    if( NULL == pMsg )
    {
        return gPhyInvalidParameter_c;
    }

    switch( pMsg->msgType )
    {
    case gPlmeEdReq_c:
    case gPlmeCcaReq_c:
        MSG_Queue(&phyLocal[phyInstance].macPhyInputQueue, pMsg);
        Phy24Task( &phyLocal[phyInstance] );
        break;

    case gPlmeSetReq_c:
#if gMpmIncluded_d
        result = MPM_SetPIB(pMsg->msgData.setReq.PibAttribute,
                            &pMsg->msgData.setReq.PibAttributeValue,
                            panIdx );
        if( !MPM_isPanActive(panIdx) )
        {
            return result;
        }
#endif
        return PhyPlmeSetPIBRequest(pMsg->msgData.setReq.PibAttribute, pMsg->msgData.setReq.PibAttributeValue, phyRegSet, phyInstance);

    case gPlmeGetReq_c:
#if gMpmIncluded_d
        if( gPhySuccess_c == MPM_GetPIB(pMsg->msgData.getReq.PibAttribute, pMsg->msgData.getReq.pPibAttributeValue, panIdx) )
        {
            break;
        }
#endif
        return PhyPlmeGetPIBRequest( pMsg->msgData.getReq.PibAttribute, pMsg->msgData.getReq.pPibAttributeValue, phyRegSet, phyInstance);

    case gPlmeSetTRxStateReq_c:
        if(gPhySetRxOn_c == pMsg->msgData.setTRxStateReq.state)
        {
            if( PhyIsIdleRx(phyInstance) )
            {
                PhyPlmeForceTrxOffRequest();
            }
            else if( gIdle_c != PhyGetSeqState() )
            {
                return gPhyBusy_c;
            }
#if gMpmIncluded_d
            /* If another PAN has the RxOnWhenIdle PIB set, enable the DualPan Auto mode */
            if( gPhySuccess_c != MPM_PrepareForRx( pMsg->macInstance ) )
            {
                return gPhyBusy_c;
            }
#endif

            pPhyStruct->flags &= ~(gPhyFlagIdleRx_c);

            pPhyStruct->rxParams.timeStamp = pMsg->msgData.setTRxStateReq.startTime;
            pPhyStruct->rxParams.duration  = pMsg->msgData.setTRxStateReq.rxDuration;
            pPhyStruct->rxParams.phyRxMode = pMsg->msgData.setTRxStateReq.slottedMode;

            return PhyPlmeTimmedRxRequest( &pPhyStruct->rxParams );
        }
        else if (gPhyForceTRxOff_c == pMsg->msgData.setTRxStateReq.state)
        {
#if gMpmIncluded_d
            if( !MPM_isPanActive(panIdx) )
            {
                return gPhySuccess_c;
            }
#endif
            pPhyStruct->flags &= ~(gPhyFlagIdleRx_c);
            PhyPlmeForceTrxOffRequest();
        }
        break;

    default:
        return gPhyInvalidPrimitive_c;
    }

    return gPhySuccess_c;
}

/*! *********************************************************************************
* \brief  This function programs a new TX sequence
*
* \param[in]  pMsg Pointer to the PD request message
* \param[in]  pPhyData pointer to PHY data
*
* \return  The status of the operation.
*
********************************************************************************** */
static phyStatus_t Phy_HandlePdDataReq( Phy_PhyLocalStruct_t *pPhyData, macToPdDataMessage_t * pMsg )
{
    phyStatus_t status = gPhySuccess_c;
    phyTime_t time;
    
    if( NULL == pMsg->msgData.dataReq.pPsdu )
    {
        return gPhyInvalidParameter_c;
    }
    
    ProtectFromXcvrInterrupt();

    if( pMsg->msgData.dataReq.startTime != gPhySeqStartAsap_c )
    {
        PhyTimeSetEventTrigger( pMsg->msgData.dataReq.startTime );
    }

    status = PhyPdDataRequest(&pMsg->msgData.dataReq , &pPhyData->rxParams, &pPhyData->txParams);

    time = PhyTime_GetTimestamp();

    if( time > pMsg->msgData.dataReq.startTime )
    {
        status = gPhyTRxOff_c;
    }
    else if( pMsg->msgData.dataReq.txDuration != 0xFFFFFFFF )
    {
        if( pMsg->msgData.dataReq.startTime != gPhySeqStartAsap_c )
        {
            time = pMsg->msgData.dataReq.startTime + pMsg->msgData.dataReq.txDuration;
        }
        else
        {
            time += pMsg->msgData.dataReq.txDuration;
        }
        /* Compensate PHY overhead, including WU time */
        time += 54;
        PhyTimeSetEventTimeout( &time );
    }

    UnprotectFromXcvrInterrupt();

    if( gPhySuccess_c != status )
    {
        PhyPlmeForceTrxOffRequest();
    }

    return status;
}

/*! *********************************************************************************
* \brief  This function sets the start time and the timeout value for a sequence.
*
* \param[in]  startTime The absolute start time for the sequence.
*             If startTime is gPhySeqStartAsap_c, the start timer is disabled.
* \param[in]  seqDuration The duration of the sequence.
*             If seqDuration is 0xFFFFFFFF, the timeout is disabled.
*
********************************************************************************** */
void Phy_SetSequenceTiming(phyTime_t startTime, uint32_t seqDuration)
{
    phyTime_t endTime;

    OSA_InterruptDisable();

    if( gPhySeqStartAsap_c == startTime )
    {
        PhyTimeReadClock( &endTime );
    }
    else
    {
        PhyTimeSetEventTrigger( startTime );
        endTime = startTime & gPhyTimeMask_c;
    }

    if( 0xFFFFFFFFU != seqDuration )
    {
        endTime += seqDuration;
        endTime = endTime & gPhyTimeMask_c;

        PhyTimeSetEventTimeout( &(endTime) );
    }

    OSA_InterruptEnable();
}

/*! *********************************************************************************
* \brief  This function starts the IdleRX if the PhyRxOnWhenIdle PIB is set
*
* \param[in]  pPhyData pointer to PHY data
*
********************************************************************************** */
void Phy_EnterIdle( Phy_PhyLocalStruct_t *pPhyData )
{
    if( (pPhyData->flags & gPhyFlagRxOnWhenIdle_c)
#if gMpmIncluded_d
       /* Prepare the Active PAN/PANs */
       && (gPhySuccess_c == MPM_PrepareForRx(gInvalidInstanceId_c))
#endif
      )
    {
        pPhyData->flags |= gPhyFlagIdleRx_c;

        pPhyData->rxParams.timeStamp = gPhySeqStartAsap_c;
        pPhyData->rxParams.duration  = gPhyMaxIdleRxDuration_c;
        pPhyData->rxParams.phyRxMode = gPhyUnslottedMode_c;

        PhyPlmeTimmedRxRequest( &pPhyData->rxParams );
    }
    else
    {
        pPhyData->flags &= ~(gPhyFlagIdleRx_c);
        if( mXcvrDisallowSleep && (gIdle_c == PhyGetSeqState()) )
        {
            mXcvrDisallowSleep = 0;
            PWR_AllowXcvrToSleep();
        }
    }
}

/*! *********************************************************************************
* \brief  This function sets the value of the maxFrameWaitTime PIB
*
* \param[in]  instanceId The instance of the PHY
* \param[in]  time The maxFrameWaitTime value
*
********************************************************************************** */
void PhyPlmeSetFrameWaitTime( uint32_t time, instanceId_t instanceId )
{
    phyLocal[instanceId].maxFrameWaitTime = time;
}

/*! *********************************************************************************
* \brief  This function sets the state of the PhyRxOnWhenIdle PIB
*
* \param[in]  instanceId The instance of the PHY
* \param[in]  state The PhyRxOnWhenIdle value
*
********************************************************************************** */
void PhyPlmeSetRxOnWhenIdle( bool_t state, instanceId_t instanceId )
{
    uint8_t radioState = PhyGetSeqState();
#if gMpmIncluded_d
    /* Check if at least one PAN has RxOnWhenIdle set */
    if( FALSE == state )
    {
        uint32_t i;

        for( i=0; i<gMpmMaxPANs_c; i++ )
        {
            MPM_GetPIB( gPhyPibRxOnWhenIdle, &state, i );
            if( state )
                break;
        }
    }
#endif

    if( state )
    {
        phyLocal[instanceId].flags |= gPhyFlagRxOnWhenIdle_c;
        if( radioState == gIdle_c)
        {
            Phy_EnterIdle( &phyLocal[instanceId] );
        }
#if gMpmIncluded_d
        else if( (radioState == gRX_c) && (phyLocal[instanceId].flags & gPhyFlagIdleRx_c) )
        {
            PhyPlmeForceTrxOffRequest();
            Phy_EnterIdle( &phyLocal[instanceId] );
        }
#endif
    }
    else
    {
        ProtectFromXcvrInterrupt();
        phyLocal[instanceId].flags &= ~gPhyFlagRxOnWhenIdle_c;
        if( (radioState == gRX_c) && (phyLocal[instanceId].flags & gPhyFlagIdleRx_c) )
        {
            PhyPlmeForceTrxOffRequest();
            MEM_BufferFree(phyLocal[instanceId].rxParams.pRxData);
            phyLocal[instanceId].rxParams.pRxData = NULL;
            phyLocal[instanceId].rxParams.pDataPending = NULL;
            phyLocal[instanceId].flags &= ~gPhyFlagIdleRx_c;
        }
        UnprotectFromXcvrInterrupt();
    }
}

/*! *********************************************************************************
* \brief  This function starts the IdleRX if the PhyRxOnWhenIdle PIB is set
*
* \param[in]  instanceId The instance of the PHY
*
********************************************************************************** */
bool_t PhyIsIdleRx( instanceId_t instanceId )
{
    if( (phyLocal[instanceId].flags & gPhyFlagIdleRx_c) && (gRX_c == PhyGetSeqState()))
        return TRUE;

    return FALSE;
}

/*! *********************************************************************************
* \brief  This function signals the PHY task that a TX operation completed successfully.
*         If the received ACK has FP=1, then the radio will enter RX state for
*         maxFrameWaitTime duration.
*
* \param[in]  instanceId The instance of the PHY
* \param[in]  framePending The value of the framePending bit for the received ACK
*
********************************************************************************** */
void Radio_Phy_PdDataConfirm(instanceId_t instanceId, bool_t framePending)
{
    if( framePending )
    {
        phyLocal[instanceId].flags |= gPhyFlagRxFP_c;
        if( phyLocal[instanceId].maxFrameWaitTime > 0 )
        {
            /* Restart Rx asap if an ACK with FP=1 is received */
            phyLocal[instanceId].flags &= ~(gPhyFlagIdleRx_c);

            phyLocal[instanceId].rxParams.timeStamp = gPhySeqStartAsap_c;
            phyLocal[instanceId].rxParams.duration  = phyLocal[instanceId].maxFrameWaitTime;
            phyLocal[instanceId].rxParams.phyRxMode = gPhyUnslottedMode_c;

            PhyPlmeTimmedRxRequest(&phyLocal[instanceId].rxParams);
        }
    }
    else
    {
        phyLocal[instanceId].flags &= ~gPhyFlagRxFP_c;
    }

    PD_SendMessage(&phyLocal[instanceId], gPdDataCnf_c);
}

/*! *********************************************************************************
* \brief  This function signals the PHY task that new data has been received
*
* \param[in]  instanceId The instance of the PHY
*
********************************************************************************** */
void Radio_Phy_PdDataIndication(instanceId_t instanceId)
{
    PD_SendMessage(&phyLocal[instanceId], gPdDataInd_c);
}

/*! *********************************************************************************
* \brief  This function signals the PHY task that timer1 compare match occured
*
* \param[in]  instanceId The instance of the PHY
*
********************************************************************************** */
void Radio_Phy_TimeWaitTimeoutIndication(instanceId_t instanceId)
{
    PhyTime_ISR();
}

/*! *********************************************************************************
* \brief  This function signals the PHY task that a CCA sequence has finished
*
* \param[in]  instanceId The instance of the PHY
* \param[in]  phyChannelStatus The status of the channel: Idle/Busy
*
* \return  None.
*
********************************************************************************** */
void Radio_Phy_PlmeCcaConfirm(phyStatus_t phyChannelStatus, instanceId_t instanceId)
{
    phyLocal[instanceId].channelParams.channelStatus = phyChannelStatus;

    PLME_SendMessage(&phyLocal[instanceId], gPlmeCcaCnf_c);
}

/*! *********************************************************************************
* \brief  This function signals the PHY task that a ED sequence has finished
*
* \param[in]  instanceId The instance of the PHY
* \param[in]  energyLevel The enetgy level on the channel.
* \param[in]  energyLeveldB The energy level in DB
*
********************************************************************************** */
void Radio_Phy_PlmeEdConfirm(uint8_t energyLeveldB, instanceId_t instanceId)
{
    phyLocal[instanceId].channelParams.energyLeveldB = energyLeveldB;

    PLME_SendMessage(&phyLocal[instanceId], gPlmeEdCnf_c);
}

/*! *********************************************************************************
* \brief  This function signals the PHY task that the programmed sequence has timed out
*         The Radio is forced to Idle.
*
* \param[in]  instanceId The instance of the PHY
*
********************************************************************************** */
void Radio_Phy_TimeRxTimeoutIndication(instanceId_t instanceId)
{
    if( !(phyLocal[instanceId].flags & gPhyFlagIdleRx_c) )
    {
        PLME_SendMessage(&phyLocal[instanceId], gPlmeTimeoutInd_c);
    }
}

/*! *********************************************************************************
* \brief  This function signals the PHY task that the programmed sequence has started
*
* \param[in]  instanceId The instance of the PHY
*
* \return  None.
*
********************************************************************************** */
void Radio_Phy_TimeStartEventIndication(instanceId_t instanceId)
{
#ifdef MAC_PHY_DEBUG
    PLME_SendMessage(&phyLocal[instanceId], gPlme_StartEventInd_c);
#endif
}

/*! *********************************************************************************
* \brief  This function signals the PHY task that a SFD was detected.
*         Also, if there is not enough time to receive the entire packet, the
*         RX timeout will be extended.
*
* \param[in]  instanceId The instance of the PHY
* \param[in]  frameLen the length of the PSDU
*
********************************************************************************** */
void Radio_Phy_PlmeRxSfdDetect(instanceId_t instanceId, uint32_t frameLen)
{
    if( (phyLocal[instanceId].flags & gPhyFlagDeferTx_c) && (frameLen <= gMaxPHYPacketSize_c) )
    {
        uint8_t phyReg;
        phyTime_t currentTime, time;

        frameLen = frameLen * 2 + 12 + 54 + 2; /* Convert to symbols and add IFS and ACK duration */
#if gPhyUseReducedSpiAccess_d
        /* Read currentTime and Timeout values [sym] */
        PhyTimeReadClock(&currentTime);

        time = (mPhySeqTimeout - currentTime) & gPhyTimeMask_c;

        if( time > 3 )
        {
            /* disable TMR3 IRQ */
            phyReg = MCR20Drv_DirectAccessSPIRead(IRQSTS3);
            phyReg &= 0xF0;
            phyReg |= cIRQSTS3_TMR3MSK;
            MCR20Drv_DirectAccessSPIWrite(IRQSTS3, phyReg);
            /* write new TMR3 compare value */
            mPhySeqTimeout = (currentTime + frameLen) & gPhyTimeMask_c;
            MCR20Drv_DirectAccessSPIMultiByteWrite( T3CMP_LSB, (uint8_t *)&mPhySeqTimeout, 3);
            /* enable TMR3 IRQ */
            phyReg &= ~cIRQSTS3_TMR3MSK;
            phyReg |= cIRQSTS3_TMR3IRQ;
            MCR20Drv_DirectAccessSPIWrite(IRQSTS3, phyReg);
        }
#else
        OSA_InterruptDisable();
        
        /* Read currentTime and Timeout values [sym] */
        PhyTimeReadClock(&currentTime);

        time = (mPhySeqTimeout - currentTime) & gPhyTimeMask_c;
        
        if( time > frameLen )
        {
            /* Relaxed timing. No need for critical section. */
            OSA_InterruptEnable();
        }

        if( time > 3 )
        {
            /* disable TMR3 compare */
            phyReg = MCR20Drv_DirectAccessSPIRead(PHY_CTRL3);
            phyReg &= ~(cPHY_CTRL3_TMR3CMP_EN);
            MCR20Drv_DirectAccessSPIWrite( PHY_CTRL3, phyReg);
            /* write new TMR3 compare value */
            mPhySeqTimeout = (currentTime + frameLen) & gPhyTimeMask_c;
            MCR20Drv_DirectAccessSPIMultiByteWrite( T3CMP_LSB, (uint8_t *)&mPhySeqTimeout, 3);
            /* enable TMR3 compare */
            phyReg |= cPHY_CTRL3_TMR3CMP_EN;
            MCR20Drv_DirectAccessSPIWrite( PHY_CTRL3, phyReg);
        }

        if( time <= frameLen )
        {
            OSA_InterruptEnable();
        }
#endif
    }

#ifdef MAC_PHY_DEBUG
    PLME_SendMessage(&phyLocal[instanceId], gPlme_RxSfdDetectInd_c);
#endif
}

/*! *********************************************************************************
* \brief  This function signals the PHY task that a Sync Loss occured (PLL unlock)
*         The Radio is forced to Idle.
*
* \param[in]  instanceId The instance of the PHY
*
********************************************************************************** */
void Radio_Phy_PlmeSyncLossIndication(instanceId_t instanceId)
{
    PhyPlmeForceTrxOffRequest();
#ifdef MAC_PHY_DEBUG
    PLME_SendMessage(&phyLocal[instanceId], gPlme_SyncLossInd_c);
#endif
    Radio_Phy_TimeRxTimeoutIndication(instanceId);
}

/*! *********************************************************************************
* \brief  This function signals the PHY task that a Filter Fail occured
*
* \param[in]  instanceId The instance of the PHY
*
********************************************************************************** */
void Radio_Phy_PlmeFilterFailRx(instanceId_t instanceId)
{
    if( phyLocal[instanceId].flags & gPhyFlaqReqPostponed_c )
    {
        /* The Rx packet is not intended for the current device.
         * Signal a Channel Busy event, and discard the Tx request */
        phyLocal[instanceId].flags &= ~(gPhyFlaqReqPostponed_c);
        if( MSG_DeQueue(&phyLocal[instanceId].macPhyInputQueue) )
        {
            Radio_Phy_PlmeCcaConfirm(gPhyChannelBusy_c, instanceId);
        }
    }
#ifdef MAC_PHY_DEBUG
    PLME_SendMessage(&phyLocal[instanceId], gPlme_FilterFailInd_c);
#endif
}

/*! *********************************************************************************
* \brief  This function signals the PHY task that an unexpected Transceiver Reset
*          occured and force the TRX to Off
*
* \param[in]  instanceId The instance of the PHY
*
********************************************************************************** */
void Radio_Phy_UnexpectedTransceiverReset(instanceId_t instanceId)
{
    PhyPlmeForceTrxOffRequest();
#ifdef MAC_PHY_DEBUG
    PLME_SendMessage(&phyLocal[instanceId], gPlme_UnexpectedRadioResetInd_c);
#endif
    Radio_Phy_TimeRxTimeoutIndication(instanceId);
}

/*! *********************************************************************************
* \brief  Senf a PLME message to upper layer
*
* \param[in]  instanceId The instance of the PHY
* \param[in]  msgType    The type of message to be sent
*
********************************************************************************** */
static void PLME_SendMessage(Phy_PhyLocalStruct_t *pPhyStruct, phyMessageId_t msgType)
{
    plmeToMacMessage_t * pMsg;

    pPhyStruct->flags &= ~(gPhyFlaqReqPostponed_c);

    if( pPhyStruct->PLME_MAC_SapHandler )
    {
        pMsg = Phy_BufferAlloc(sizeof(plmeToMacMessage_t));
    
        if(NULL == pMsg)
        {
            phyTimeEvent_t ev = {
                .parameter = (uint32_t)msgType,
                .callback = Phy_SendLatePLME,
                .timestamp = gPhyRxRetryInterval_c + PhyTime_GetTimestamp()
            };
            
            PhyTime_ScheduleEvent(&ev);
            return;
        }

        pMsg->msgType = msgType;

        switch(msgType)
        {
        case gPlmeCcaCnf_c:
            pMsg->msgData.ccaCnf.status = pPhyStruct->channelParams.channelStatus;
            break;

        case gPlmeEdCnf_c:
            pMsg->msgData.edCnf.status        = gPhySuccess_c;
            pMsg->msgData.edCnf.energyLeveldB = pPhyStruct->channelParams.energyLeveldB;
            pMsg->msgData.edCnf.energyLevel   = Phy_GetEnergyLevel(pPhyStruct->channelParams.energyLeveldB);
            break;

        default:
            /* No aditional info needs to be filled */
            break;
        }

        pPhyStruct->PLME_MAC_SapHandler(pMsg, pPhyStruct->currentMacInstance);
    }
}

/*! *********************************************************************************
* \brief  Senf a PD message to upper layer
*
* \param[in]  instanceId The instance of the PHY
* \param[in]  msgType    The type of message to be sent
*
********************************************************************************** */
static void PD_SendMessage(Phy_PhyLocalStruct_t *pPhyStruct, phyMessageId_t msgType)
{
    pdDataToMacMessage_t *pMsg;
    
    if( pPhyStruct->PD_MAC_SapHandler )
    {
        if( msgType == gPdDataInd_c )
        {
            uint32_t temp;
            uint16_t len = pPhyStruct->rxParams.psduLength - 2; /* Excluding FCS (2 bytes); */

            if( NULL == pPhyStruct->rxParams.pDataPending )
            {
                return;
            }

            pMsg = pPhyStruct->rxParams.pDataPending;
            pPhyStruct->rxParams.pDataPending = NULL;
            
            /* Check for corrupt packets */
            if( len > (gMaxPHYPacketSize_c - 2) ) 
            {
                MEM_BufferFree(pMsg);
                return;
            }

            /* If gPhyRxPBTransferThereshold_d is enabled, the packet is transfered in RAM 
            when the specified threshold is reached. Else, the payload is read below */
            if( NULL == pMsg->msgData.dataInd.pPsdu )
            {
                pMsg->msgData.dataInd.pPsdu = (uint8_t*)&pMsg->msgData.dataInd.pPsdu +
                                              sizeof(pMsg->msgData.dataInd.pPsdu);
                MCR20Drv_PB_SPIBurstRead( (uint8_t *)(pMsg->msgData.dataInd.pPsdu), len );
            }
            
            if( pPhyStruct->flags & gPhyFlaqReqPostponed_c )
            {
                uint8_t *pPsdu = pMsg->msgData.dataInd.pPsdu;
                uint8_t  dstAddrMode = (pPsdu[mFrameCtrlHi_d] & mDstAddrModeMask_d) >> mDstAddrModeShift_d;

                /* Skip over FrameControl, SeqNo and PanID */
                pPsdu += mAddressingFields_d + 2;
                    
                if( (dstAddrMode == mShortAddr_d) && (pPsdu[0] == 0xFF) && (pPsdu[1] == 0xFF) )
                {
                    /* The Rx packet is a broadcast message.
                     * Signal a Channel Busy event, and discard the Tx request */
                    pPhyStruct->flags &= ~(gPhyFlaqReqPostponed_c);
                    if( MSG_DeQueue(&pPhyStruct->macPhyInputQueue) )
                    {
                        pPhyStruct->channelParams.channelStatus = gPhyChannelBusy_c;
                        PLME_SendMessage(pPhyStruct, gPlmeCcaCnf_c);
                    }
                }
            }

            pMsg->msgType                         = gPdDataInd_c;
            pMsg->msgData.dataInd.ppduLinkQuality = pPhyStruct->rxParams.linkQuality;
            pMsg->msgData.dataInd.psduLength      = len;

            pMsg->msgData.dataInd.timeStamp       = PhyTime_GetTimestamp();      /* current timestamp (64bit) */
            temp = (uint32_t)(pMsg->msgData.dataInd.timeStamp & gPhyTimeMask_c); /* convert to 24bit */
            pMsg->msgData.dataInd.timeStamp -= (temp - pPhyStruct->rxParams.timeStamp) & gPhyTimeMask_c;
    #if !(gMpmIncluded_d)
            pPhyStruct->PD_MAC_SapHandler(pMsg, pPhyStruct->currentMacInstance);
    #else
            {
                uint32_t i, bitMask = PhyPpGetPanOfRxPacket();

                for( i=0; i<gMpmPhyPanRegSets_c; i++ )
                {
                    if( bitMask & (1 << i) )
                    {
                        bitMask &= ~(1 << i);
                        pPhyStruct->currentMacInstance = MPM_GetMacInstanceFromRegSet(i);

                        /* If the packet passed filtering on muliple PANs, send a copy to each one */
                        if( bitMask )
                        {
                            pdDataToMacMessage_t *pDataIndCopy;

                            pDataIndCopy = Phy_BufferAlloc(sizeof(pdDataToMacMessage_t) + len);
                            if( pDataIndCopy )
                            {
                                FLib_MemCpy(pDataIndCopy, pMsg, sizeof(pdDataToMacMessage_t) + len);
                                pPhyStruct->PD_MAC_SapHandler(pDataIndCopy, pPhyStruct->currentMacInstance);
                            }
                        }
                        else
                        {
                            pPhyStruct->PD_MAC_SapHandler(pMsg, pPhyStruct->currentMacInstance);
                            break;
                        }
                    }
                }
            }
    #endif
        }
        else
        {
            phyStatus_t status;

            if( (pPhyStruct->flags & gPhyFlagRxFP_c) == gPhyFlagRxFP_c )
            {
                pPhyStruct->flags &= ~(gPhyFlagRxFP_c);
                status = gPhyFramePending_c;
            }
            else
            {
                status = gPhySuccess_c;
            }

            pPhyStruct->flags &= ~(gPhyFlaqReqPostponed_c);
            pMsg = Phy_BufferAlloc(sizeof(phyMessageHeader_t) + sizeof(pdDataCnf_t));

            if(NULL == pMsg)
            {
                phyTimeEvent_t ev = {
                    .callback = Phy_SendLatePD,
                    .parameter = (uint32_t)msgType,
                    .timestamp = gPhyRxRetryInterval_c + PhyTime_GetTimestamp()
                };
                
                PhyTime_ScheduleEvent(&ev);
                return;
            }

            pMsg->msgType = gPdDataCnf_c;
            pMsg->msgData.dataCnf.status = status;
            pPhyStruct->PD_MAC_SapHandler(pMsg, pPhyStruct->currentMacInstance);
        }
    }
}


static void Phy_SendLatePLME( uint32_t param )
{
    PLME_SendMessage(&phyLocal[0], (phyMessageId_t)param);
}

static void Phy_SendLatePD( uint32_t param )
{
    PD_SendMessage(&phyLocal[0], (phyMessageId_t)param);
}

void Radio_Phy_Notify(instanceId_t instanceId)
{
    Phy24Task(&phyLocal[instanceId]);
}

#if gMWS_UseCoexistence_d
/*! *********************************************************************************
* \brief  This function represents the callback used by the MWS module to signal
*         events to the 802.15.4 PHY
*
* \param[in]  event - the MWS event
*
* \return  status
*
********************************************************************************** */
static uint32_t MWS_802_15_4_Callback ( mwsEvents_t event )
{
    uint32_t status = 0;
    
    if( gMWS_Abort_c == event )
    {
        mCoexAbortPending = 1;
    }
    
    MCR20Drv_ForceIrqPending();
    return status;
}
#endif