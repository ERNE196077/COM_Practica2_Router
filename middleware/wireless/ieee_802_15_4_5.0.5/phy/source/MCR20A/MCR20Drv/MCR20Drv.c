/*!
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* All rights reserved.
*
* \file MCR20Drv.c
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
#include "MCR20Drv.h"
#include "MCR20Reg.h"

#include "SPI.h"
#include "fsl_gpio.h"
#include "gpio_pins.h"
#include "fsl_os_abstraction.h"

/*! *********************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
********************************************************************************** */
#define mMCR20SpiWriteSpeed_c (16000000)
#define mMCR20SpiReadSpeed_c   (8000000)


/*! *********************************************************************************
*************************************************************************************
* Private variables
*************************************************************************************
********************************************************************************** */
static uint32_t mPhyIrqDisableCnt = 1;


/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

/*---------------------------------------------------------------------------
* Name: MCR20Drv_Init
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_Init
(
void
)
{
    spi_master_init(gXcvrSpiInstance_c);
    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiReadSpeed_c);

    /* Override SPI CS pin function. Set pin as GPIO */
    GpioSetPinMux(mXcvrSpiCsCfg.gpioPort, mXcvrSpiCsCfg.gpioPin, pinMux_Gpio_c);
    GpioOutputPinInit(&mXcvrSpiCsCfg, 1);
    GpioOutputPinInit(&mXcvrResetPinCfg, 1);
    gXcvrDeassertCS_d();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_DirectAccessSPIWrite
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_DirectAccessSPIWrite
(
uint8_t address,
uint8_t value
)
{
    uint16_t txData;

    ProtectFromMCR20Interrupt();

    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiWriteSpeed_c);

    gXcvrAssertCS_d();

    txData = (address & TransceiverSPI_DirectRegisterAddressMask);
    txData |= value << 8;

    spi_master_transfer(gXcvrSpiInstance_c, (uint8_t *)&txData, NULL, sizeof(txData));

    gXcvrDeassertCS_d();
    UnprotectFromMCR20Interrupt();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_DirectAccessSPIMultiByteWrite
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_DirectAccessSPIMultiByteWrite
(
uint8_t startAddress,
uint8_t * byteArray,
uint8_t numOfBytes
)
{
    uint8_t txData;

    if( (numOfBytes == 0) || (byteArray == NULL) )
    {
        return;
    }

    ProtectFromMCR20Interrupt();

    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiWriteSpeed_c);

    gXcvrAssertCS_d();

    txData = (startAddress & TransceiverSPI_DirectRegisterAddressMask);

    spi_master_transfer(gXcvrSpiInstance_c, &txData, NULL, sizeof(txData));
    spi_master_transfer(gXcvrSpiInstance_c, byteArray, NULL, numOfBytes);

    gXcvrDeassertCS_d();
    UnprotectFromMCR20Interrupt();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_PB_SPIByteWrite
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_PB_SPIByteWrite
(
uint8_t address,
uint8_t * byteArray,
uint8_t numOfBytes
)
{
    uint16_t txData;

    ProtectFromMCR20Interrupt();

    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiWriteSpeed_c);

    gXcvrAssertCS_d();

    txData  = TransceiverSPI_WriteSelect            |
              TransceiverSPI_PacketBuffAccessSelect |
              TransceiverSPI_PacketBuffByteModeSelect;
    txData |= (address) << 8;

    spi_master_transfer(gXcvrSpiInstance_c, (uint8_t*)&txData, NULL, 2);
    spi_master_transfer(gXcvrSpiInstance_c, byteArray, NULL, numOfBytes);

    gXcvrDeassertCS_d();
    UnprotectFromMCR20Interrupt();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_PB_SPIBurstWrite
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_PB_SPIBurstWrite
(
uint8_t * byteArray,
uint8_t numOfBytes
)
{
    uint8_t txData;

    if( (numOfBytes == 0) || (byteArray == NULL) )
    {
        return;
    }

    ProtectFromMCR20Interrupt();

    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiWriteSpeed_c);

    gXcvrAssertCS_d();

    txData = TransceiverSPI_WriteSelect            |
             TransceiverSPI_PacketBuffAccessSelect |
             TransceiverSPI_PacketBuffBurstModeSelect;

    spi_master_transfer(gXcvrSpiInstance_c, &txData, NULL, 1);
    spi_master_transfer(gXcvrSpiInstance_c, byteArray, NULL, numOfBytes);

    gXcvrDeassertCS_d();
    UnprotectFromMCR20Interrupt();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_DirectAccessSPIRead
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/

uint8_t MCR20Drv_DirectAccessSPIRead
(
uint8_t address
)
{
    uint8_t txData;
    uint8_t rxData;

    ProtectFromMCR20Interrupt();

    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiReadSpeed_c);

    gXcvrAssertCS_d();

    txData = (address & TransceiverSPI_DirectRegisterAddressMask) |
             TransceiverSPI_ReadSelect;

    spi_master_transfer(gXcvrSpiInstance_c, &txData, NULL, sizeof(txData));
    spi_master_transfer(gXcvrSpiInstance_c, NULL, &rxData, sizeof(rxData));

    gXcvrDeassertCS_d();

    UnprotectFromMCR20Interrupt();

    return rxData;

}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_DirectAccessSPIMultyByteRead
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
uint8_t MCR20Drv_DirectAccessSPIMultiByteRead
(
uint8_t startAddress,
uint8_t * byteArray,
uint8_t numOfBytes
)
{
    uint8_t  txData;
    uint8_t  phyIRQSTS1;

    if( (numOfBytes == 0) || (byteArray == NULL) )
    {
        return 0;
    }

    ProtectFromMCR20Interrupt();

    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiReadSpeed_c);

    gXcvrAssertCS_d();

    txData = (startAddress & TransceiverSPI_DirectRegisterAddressMask) |
             TransceiverSPI_ReadSelect;

    spi_master_transfer(gXcvrSpiInstance_c, &txData, &phyIRQSTS1, sizeof(txData));
    spi_master_transfer(gXcvrSpiInstance_c, NULL, byteArray, numOfBytes);

    gXcvrDeassertCS_d();
    UnprotectFromMCR20Interrupt();

    return phyIRQSTS1;
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_PB_SPIBurstRead
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
uint8_t MCR20Drv_PB_SPIBurstRead
(
uint8_t * byteArray,
uint8_t numOfBytes
)
{
    uint8_t  txData;
    uint8_t  phyIRQSTS1;

    if( (numOfBytes == 0) || (byteArray == NULL) )
    {
        return 0;
    }

    ProtectFromMCR20Interrupt();

    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiReadSpeed_c);

    gXcvrAssertCS_d();

    txData = TransceiverSPI_ReadSelect |
             TransceiverSPI_PacketBuffAccessSelect |
             TransceiverSPI_PacketBuffBurstModeSelect;

    spi_master_transfer(gXcvrSpiInstance_c, &txData, &phyIRQSTS1, sizeof(txData));
    spi_master_transfer(gXcvrSpiInstance_c, NULL, byteArray, numOfBytes);

    gXcvrDeassertCS_d();
    UnprotectFromMCR20Interrupt();

    return phyIRQSTS1;
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_PB_SPIByteRead
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_PB_SPIByteRead
(
uint8_t address,
uint8_t * byteArray,
uint8_t numOfBytes
)
{
    uint16_t txData;

    ProtectFromMCR20Interrupt();

    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiReadSpeed_c);

    gXcvrAssertCS_d();

    txData  = TransceiverSPI_WriteSelect            |
              TransceiverSPI_PacketBuffAccessSelect |
              TransceiverSPI_PacketBuffByteModeSelect;
    txData |= (address) << 8;

    spi_master_transfer(gXcvrSpiInstance_c, (uint8_t*)&txData, NULL, 2);
    spi_master_transfer(gXcvrSpiInstance_c, NULL, byteArray, numOfBytes);

    gXcvrDeassertCS_d();
    UnprotectFromMCR20Interrupt();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_IndirectAccessSPIWrite
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_IndirectAccessSPIWrite
(
uint8_t address,
uint8_t value
)
{
    uint32_t  txData;

    ProtectFromMCR20Interrupt();

    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiWriteSpeed_c);

    gXcvrAssertCS_d();

    txData = TransceiverSPI_IARIndexReg;
    txData |= (address) << 8;
    txData |= (value)   << 16;

    spi_master_transfer(gXcvrSpiInstance_c, (uint8_t*)&txData, NULL, 3);

    gXcvrDeassertCS_d();
    UnprotectFromMCR20Interrupt();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_IndirectAccessSPIMultiByteWrite
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_IndirectAccessSPIMultiByteWrite
(
uint8_t startAddress,
uint8_t * byteArray,
uint8_t numOfBytes
)
{
    uint16_t  txData;

    if( (numOfBytes == 0) || (byteArray == NULL) )
    {
        return;
    }

    ProtectFromMCR20Interrupt();

    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiWriteSpeed_c);

    gXcvrAssertCS_d();

    txData = TransceiverSPI_IARIndexReg;
    txData |= (startAddress)  << 8;

    spi_master_transfer(gXcvrSpiInstance_c, (uint8_t*)&txData, NULL, sizeof(txData));
    spi_master_transfer(gXcvrSpiInstance_c, (uint8_t*)byteArray, NULL, numOfBytes);

    gXcvrDeassertCS_d();
    UnprotectFromMCR20Interrupt();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_IndirectAccessSPIRead
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
uint8_t MCR20Drv_IndirectAccessSPIRead
(
uint8_t address
)
{
    uint16_t  txData;
    uint8_t   rxData;

    ProtectFromMCR20Interrupt();

    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiReadSpeed_c);

    gXcvrAssertCS_d();

    txData = TransceiverSPI_IARIndexReg | TransceiverSPI_ReadSelect;
    txData |= (address) << 8;

    spi_master_transfer(gXcvrSpiInstance_c, (uint8_t*)&txData, NULL, sizeof(txData));
    spi_master_transfer(gXcvrSpiInstance_c, NULL, &rxData, sizeof(rxData));

    gXcvrDeassertCS_d();
    UnprotectFromMCR20Interrupt();

    return rxData;
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_IndirectAccessSPIMultiByteRead
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_IndirectAccessSPIMultiByteRead
(
uint8_t startAddress,
uint8_t * byteArray,
uint8_t numOfBytes
)
{
    uint16_t  txData;

    if( (numOfBytes == 0) || (byteArray == NULL) )
    {
        return;
    }

    ProtectFromMCR20Interrupt();

    spi_master_configure_speed(gXcvrSpiInstance_c, mMCR20SpiReadSpeed_c);

    gXcvrAssertCS_d();

    txData = (TransceiverSPI_IARIndexReg | TransceiverSPI_ReadSelect);
    txData |= (startAddress) << 8;

    spi_master_transfer(gXcvrSpiInstance_c, (uint8_t*)&txData, NULL, sizeof(txData));
    spi_master_transfer(gXcvrSpiInstance_c, NULL, byteArray, numOfBytes);

    gXcvrDeassertCS_d();
    UnprotectFromMCR20Interrupt();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_IRQ_PortConfig
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_IRQ_PortConfig
(
void
)
{
    GpioSetPinMux(mXcvrIrqPinCfg.gpioPort, mXcvrIrqPinCfg.gpioPin, pinMux_Gpio_c);
    GpioInputPinInit(&mXcvrIrqPinCfg, 1);
    GpioSetPassiveFilter(mXcvrIrqPinCfg.gpioPort, mXcvrIrqPinCfg.gpioPin, FALSE);
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_IsIrqPending
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
uint32_t  MCR20Drv_IsIrqPending
(
void
)
{
    if( !GpioReadPinInput(&mXcvrIrqPinCfg) )
    {
        return TRUE;
    }

    return FALSE;
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_ForceIrqPending
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_ForceIrqPending
(
void
)
{
    pinInterrupt_t mode;
    
    /* Save IRQ mode */
    mode = GpioGetPinInterrupt(mXcvrIrqPinCfg.gpioPort, mXcvrIrqPinCfg.gpioPin);
    
    OSA_InterruptDisable();
    /* Force IRQ to be set as pending */
    GpioSetPinInterrupt(mXcvrIrqPinCfg.gpioPort, mXcvrIrqPinCfg.gpioPin, pinInt_LogicOne_c);
    /* Restore IRQ mode */
    GpioSetPinInterrupt(mXcvrIrqPinCfg.gpioPort, mXcvrIrqPinCfg.gpioPin, mode);

    OSA_InterruptEnable();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_IRQ_Disable
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_IRQ_Disable
(
void
)
{
    OSA_InterruptDisable();

    if( mPhyIrqDisableCnt == 0 )
    {
        GpioSetPinInterrupt(mXcvrIrqPinCfg.gpioPort, mXcvrIrqPinCfg.gpioPin, pinInt_Disabled_c);
    }

    mPhyIrqDisableCnt++;

    OSA_InterruptEnable();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_IRQ_Enable
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_IRQ_Enable
(
void
)
{
    OSA_InterruptDisable();

    if( mPhyIrqDisableCnt )
    {
        mPhyIrqDisableCnt--;

        if( mPhyIrqDisableCnt == 0 )
        {
            GpioSetPinInterrupt(mXcvrIrqPinCfg.gpioPort, mXcvrIrqPinCfg.gpioPin, pinInt_LogicZero_c);
        }
    }

    OSA_InterruptEnable();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_IRQ_IsEnabled
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
uint32_t MCR20Drv_IRQ_IsEnabled
(
void
)
{
    pinInterrupt_t mode;

    mode = GpioGetPinInterrupt(mXcvrIrqPinCfg.gpioPort, mXcvrIrqPinCfg.gpioPin);
    return (mode != pinInt_Disabled_c);
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_IRQ_Clear
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_IRQ_Clear
(
void
)
{
    GpioClearPinIntFlag(&mXcvrIrqPinCfg);
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_RST_Assert
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_RST_B_Assert
(
void
)
{
    GpioClearPinOutput(&mXcvrResetPinCfg);
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_RST_Deassert
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_RST_B_Deassert
(
void
)
{
    GpioSetPinOutput(&mXcvrResetPinCfg);
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_SoftRST_Assert
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_SoftRST_Assert
(
void
)
{
    MCR20Drv_IndirectAccessSPIWrite(SOFT_RESET, (0x80));
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_SoftRST_Deassert
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_SoftRST_Deassert
(
void
)
{
    MCR20Drv_IndirectAccessSPIWrite(SOFT_RESET, (0x00));
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_Soft_RESET
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_Soft_RESET
(
void
)
{
    //assert SOG_RST
    MCR20Drv_IndirectAccessSPIWrite(SOFT_RESET, (0x80));

    //deassert SOG_RST
    MCR20Drv_IndirectAccessSPIWrite(SOFT_RESET, (0x00));
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_RESET
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_RESET
(
void
)
{
    volatile uint32_t delay = 100;
    //assert RST_B
    MCR20Drv_RST_B_Assert();

    while(delay--);

    //deassert RST_B
    MCR20Drv_RST_B_Deassert();
}

/*---------------------------------------------------------------------------
* Name: MCR20Drv_Set_CLK_OUT_Freq
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void MCR20Drv_Set_CLK_OUT_Freq
(
uint8_t freqDiv
)
{
    uint8_t clkOutCtrlReg = (freqDiv & cCLK_OUT_DIV_Mask) | cCLK_OUT_EN | cCLK_OUT_EXTEND;

    if(freqDiv == gCLK_OUT_FREQ_DISABLE)
    {
        clkOutCtrlReg = (cCLK_OUT_EXTEND | gCLK_OUT_FREQ_4_MHz); //reset value with clock out disabled
    }

    MCR20Drv_DirectAccessSPIWrite((uint8_t) CLK_OUT_CTRL, clkOutCtrlReg);
}

/**************************************************************************************************/
/*                                        XCVR GPIO API                                           */
/**************************************************************************************************/

/*! *********************************************************************************
* \brief  Set the XCVR GPIO pin value to logic 1 (one)
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
* \remarks Use only if the pin is set up as output
*
********************************************************************************** */
void MCR20Drv_SetGpioPin(uint8_t GpioMask)
{
    uint8_t phyReg = MCR20Drv_IndirectAccessSPIRead(GPIO_DATA);
    phyReg |= GpioMask;
    MCR20Drv_IndirectAccessSPIWrite(GPIO_DATA, phyReg);
}

/*! *********************************************************************************
* \brief  Set the XCVR GPIO pin value to logic 0 (zero)
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
* \remarks Use only if the pin is set up as output
*
********************************************************************************** */
void MCR20Drv_ClearGpioPin(uint8_t GpioMask)
{
    uint8_t phyReg = MCR20Drv_IndirectAccessSPIRead(GPIO_DATA);
    phyReg &= ~GpioMask;
    MCR20Drv_IndirectAccessSPIWrite(GPIO_DATA, phyReg);
}

/*! *********************************************************************************
* \brief  Read the XCVR GPIO pin value
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
* \return  the value of the requested GPIOs: 
*          bit0 -> value of GPIO1, ... bit7 -> value of GPIO8
*
* \remarks Use only if the pin is set up as input
*
********************************************************************************** */
uint8_t MCR20Drv_GetGpioPinValue(uint8_t GpioMask)
{
    uint8_t phyReg = MCR20Drv_IndirectAccessSPIRead(GPIO_DATA);
    return  (phyReg & GpioMask);
}

/*! *********************************************************************************
* \brief  Configure the XCVR GPIO pin as output pin
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_SetGpioPinAsOutput(uint8_t GpioMask)
{
    uint8_t phyReg = MCR20Drv_IndirectAccessSPIRead(GPIO_DIR);
    phyReg |= GpioMask;
    MCR20Drv_IndirectAccessSPIWrite(GPIO_DIR, phyReg);
}

/*! *********************************************************************************
* \brief  Configure the XCVR GPIO pin as input pin
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_SetGpioPinAsInput(uint8_t GpioMask)
{
    uint8_t phyReg = MCR20Drv_IndirectAccessSPIRead(GPIO_DIR);
    phyReg &= ~GpioMask;
    MCR20Drv_IndirectAccessSPIWrite(GPIO_DIR, phyReg);
}

/*! *********************************************************************************
* \brief  Enable XCVR GPIO pin PullUp resistor
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_EnableGpioPullUp(uint8_t GpioMask)
{
    uint8_t phyReg = MCR20Drv_IndirectAccessSPIRead(GPIO_PUL_SEL);
    phyReg |= GpioMask;
    MCR20Drv_IndirectAccessSPIWrite(GPIO_PUL_SEL, phyReg);
        
    phyReg = MCR20Drv_IndirectAccessSPIRead(GPIO_PUL_EN);
    phyReg |= GpioMask;
    MCR20Drv_IndirectAccessSPIWrite(GPIO_PUL_EN, phyReg);
}

/*! *********************************************************************************
* \brief  Enable XCVR GPIO pin PullDown resistor
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_EnableGpioPullDown(uint8_t GpioMask)
{
    uint8_t phyReg = MCR20Drv_IndirectAccessSPIRead(GPIO_PUL_SEL);
    phyReg &= ~GpioMask;
    MCR20Drv_IndirectAccessSPIWrite(GPIO_PUL_SEL, phyReg);
        
    phyReg = MCR20Drv_IndirectAccessSPIRead(GPIO_PUL_EN);
    phyReg |= GpioMask;
    MCR20Drv_IndirectAccessSPIWrite(GPIO_PUL_EN, phyReg);
}

/*! *********************************************************************************
* \brief  Disable XCVR GPIO pin PullUp/PullDown resistors
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_DisableGpioPullUpDown(uint8_t GpioMask)
{
    uint8_t phyReg = MCR20Drv_IndirectAccessSPIRead(GPIO_PUL_EN);
    phyReg &= ~GpioMask;
    MCR20Drv_IndirectAccessSPIWrite(GPIO_PUL_EN, phyReg);
}

/*! *********************************************************************************
* \brief  Enable hi drive strength on XCVR GPIO pin
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_EnableGpioHiDriveStrength(uint8_t GpioMask)
{
    uint8_t phyReg = MCR20Drv_IndirectAccessSPIRead(GPIO_DS);
    phyReg &= ~GpioMask;
    MCR20Drv_IndirectAccessSPIWrite(GPIO_DS, phyReg);
}

/*! *********************************************************************************
* \brief  Disable hi drive strength on XCVR GPIO pin
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_DisableGpioHiDriveStrength(uint8_t GpioMask)
{
    uint8_t phyReg = MCR20Drv_IndirectAccessSPIRead(GPIO_DS);
    phyReg &= ~GpioMask;
    MCR20Drv_IndirectAccessSPIWrite(GPIO_DS, phyReg);
}