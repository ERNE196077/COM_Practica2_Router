/*!
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* All rights reserved.
*
* \file MCR20Drv.h
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
#include "SPI.h"
#include "fsl_clock.h"
#include "pin_mux.h"

#if FSL_FEATURE_SOC_DSPI_COUNT
    #include "fsl_dspi.h"
#else
    #include "fsl_spi.h"
#endif


/*! *********************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
********************************************************************************** */
#if FSL_FEATURE_SOC_DSPI_COUNT
uint32_t mDspiCmd;
#else
uint8_t mSpiLowSpeed, mSpiHighSpeed;
#endif

extern SPI_Type * const mSpiBase[];


/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */
void spi_master_init(uint32_t instance)
{
    SPI_Type *baseAddr = mSpiBase[instance];
#if FSL_FEATURE_SOC_DSPI_COUNT
    dspi_master_config_t config;
    dspi_command_data_config_t cmdConfig;
#else
    spi_master_config_t config;
#endif

    /* set SPI Pin Mux */    
    BOARD_InitXCVR();

#if FSL_FEATURE_SOC_DSPI_COUNT
    DSPI_MasterGetDefaultConfig(&config);
    config.ctarConfig.baudRate = 8000000;
    config.ctarConfig.betweenTransferDelayInNanoSec = 0;
    config.ctarConfig.lastSckToPcsDelayInNanoSec = 0;
    config.ctarConfig.pcsToSckDelayInNanoSec = 0;
    DSPI_MasterInit(baseAddr, &config, BOARD_GetSpiClock(instance));
    
    config.whichCtar = kDSPI_Ctar1;
    config.ctarConfig.baudRate = 16000000;
    DSPI_MasterInit(baseAddr, &config, BOARD_GetSpiClock(instance));


   /* Initialize SPI module */
    DSPI_SetFifoEnable(baseAddr, TRUE, TRUE);

    DSPI_GetDefaultDataCommandConfig(&cmdConfig);
    mDspiCmd = DSPI_MasterGetFormattedCommand(&cmdConfig);
#else

    SPI_MasterGetDefaultConfig(&config);
    config.outputMode = kSPI_SlaveSelectAsGpio;
    config.baudRate_Bps = 8000000;
    SPI_MasterInit(baseAddr, &config, BOARD_GetSpiClock(instance));
    mSpiLowSpeed = baseAddr->BR;

    SPI_MasterSetBaudRate(baseAddr, 16000000, BOARD_GetSpiClock(instance));
    mSpiHighSpeed = baseAddr->BR;
    
#if defined(FSL_FEATURE_SPI_HAS_FIFO) && FSL_FEATURE_SPI_HAS_FIFO
    /* Disable SPI FIFO */
    baseAddr->C3 &= ~SPI_C3_FIFOMODE_MASK;
#endif
    
#endif
}

/*****************************************************************************/
/*****************************************************************************/
void spi_master_configure_speed(uint32_t instance, uint32_t freq)
{
    if( freq > 8000000 )
    {
#if FSL_FEATURE_SOC_DSPI_COUNT
        mDspiCmd |= 1 << SPI_PUSHR_CTAS_SHIFT;
#else
        mSpiBase[instance]->BR = mSpiHighSpeed;
#endif
    }
    else
    {
#if FSL_FEATURE_SOC_DSPI_COUNT
        mDspiCmd &= ~SPI_PUSHR_CTAS_MASK;
#else
        mSpiBase[instance]->BR = mSpiLowSpeed;
#endif
    }
}

/*****************************************************************************/
/*****************************************************************************/
void spi_master_transfer(uint32_t instance,
                         uint8_t * sendBuffer,
                         uint8_t * receiveBuffer,
                         size_t transferByteCount)
{
    volatile uint8_t dummy;
    SPI_Type *baseAddr = mSpiBase[instance];

    if( !transferByteCount )
        return;

    if( !sendBuffer && !receiveBuffer )
        return;

#if FSL_FEATURE_SOC_DSPI_COUNT
    DSPI_FlushFifo(baseAddr, TRUE, TRUE);
#endif

    while( transferByteCount-- )
    {
        if( sendBuffer )
        {
#if FSL_FEATURE_SOC_DSPI_COUNT
            ((uint8_t*)&mDspiCmd)[0] = *sendBuffer;
#else
            dummy = *sendBuffer;
#endif
            sendBuffer++;
        }
        else
        {
#if FSL_FEATURE_SOC_DSPI_COUNT
            ((uint8_t*)&mDspiCmd)[0] = 0;
#else
            dummy = 0;
#endif
        }

#if FSL_FEATURE_SOC_DSPI_COUNT
        DSPI_MasterWriteCommandDataBlocking(baseAddr, mDspiCmd);
        dummy = DSPI_ReadData(baseAddr);
#else
        while ((baseAddr->S & SPI_S_SPTEF_MASK) == 0) {}
        SPI_WriteData(baseAddr, dummy);
        while ((baseAddr->S & SPI_S_SPTEF_MASK) == 0) {}
#if FSL_FEATURE_SPI_16BIT_TRANSFERS
        while ((baseAddr->S & SPI_S_SPRF_MASK) == 0) {}
#endif
        dummy = (uint8_t)SPI_ReadData(baseAddr);
#endif

        if( receiveBuffer )
        {
            *receiveBuffer = dummy;
            receiveBuffer++;
        }
    }
}

/*****************************************************************************/
/*****************************************************************************/
inline void spi_master_configure_serialization_lsb(uint32_t instance)
{
    SPI_Type * baseAddr = mSpiBase[instance];
#if FSL_FEATURE_SOC_DSPI_COUNT
    baseAddr->CTAR[0] |= SPI_CTAR_LSBFE_MASK;
    baseAddr->CTAR[1] |= SPI_CTAR_LSBFE_MASK;
#else
    baseAddr->C1 |= SPI_C1_LSBFE_MASK;
#endif
}

/*****************************************************************************/
/*****************************************************************************/
inline void spi_master_configure_serialization_msb(uint32_t instance)
{
    SPI_Type * baseAddr = mSpiBase[instance];
#if FSL_FEATURE_SOC_DSPI_COUNT
    baseAddr->CTAR[0] &= ~SPI_CTAR_LSBFE_MASK;
    baseAddr->CTAR[1] &= ~SPI_CTAR_LSBFE_MASK;
#else
    baseAddr->C1 &= ~(SPI_C1_LSBFE_MASK);
#endif
}