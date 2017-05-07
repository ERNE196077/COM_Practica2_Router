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

#ifndef __MCR20_DRV_H__
#define __MCR20_DRV_H__


/*! *********************************************************************************
*************************************************************************************
* Include
*************************************************************************************
********************************************************************************** */ 
#include "EmbeddedTypes.h"
#include "GPIO_Adapter.h"

/*! *********************************************************************************
*************************************************************************************
* Public macros
*************************************************************************************
********************************************************************************** */
#ifndef gMCR20_ClkOutFreq_d 
#define gMCR20_ClkOutFreq_d gCLK_OUT_FREQ_4_MHz
#endif

#define ProtectFromMCR20Interrupt()   MCR20Drv_IRQ_Disable()
#define UnprotectFromMCR20Interrupt() MCR20Drv_IRQ_Enable()


/* MCR20A GPIO pins */
#define gMCR20_GPIO1_d (1<<0)
#define gMCR20_GPIO2_d (1<<1)
#define gMCR20_GPIO3_d (1<<2)
#define gMCR20_GPIO4_d (1<<3)
#define gMCR20_GPIO5_d (1<<4)
#define gMCR20_GPIO6_d (1<<5)
#define gMCR20_GPIO7_d (1<<6)
#define gMCR20_GPIO8_d (1<<7)


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
);

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
);

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
);

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
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_DirectAccessSPIRead
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
uint8_t MCR20Drv_DirectAccessSPIRead
(
 uint8_t address
);

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
);

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
);

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
);

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
);

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
);

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
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_IndirectAccessSPIRead
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
uint8_t MCR20Drv_IndirectAccessSPIRead
(
 uint8_t address
);
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
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_IRQ_PortConfig
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void MCR20Drv_IRQ_PortConfig
(
  void
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_IsIrqPending
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
uint32_t MCR20Drv_IsIrqPending
(
  void
);

/*---------------------------------------------------------------------------
* Name: MCR20Drv_IsIrqPending
* Description: -
* Parameters: -
* Return: -
*---------------------------------------------------------------------------*/
void  MCR20Drv_ForceIrqPending
(
  void
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_IRQ_Disable
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void MCR20Drv_IRQ_Disable
(
  void
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_IRQ_Enable
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void MCR20Drv_IRQ_Enable
(
  void
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_IRQ_IsEnabled
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
uint32_t MCR20Drv_IRQ_IsEnabled
(
  void
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_IRQ_Clear
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void MCR20Drv_IRQ_Clear
(
  void
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_RST_PortConfig
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void MCR20Drv_RST_B_PortConfig
(
  void
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_RST_Assert
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void MCR20Drv_RST_B_Assert
(
  void
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_RST_Deassert
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void MCR20Drv_RST_B_Deassert
(
  void
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_SoftRST_Assert
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void MCR20Drv_SoftRST_Assert
(
  void
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_SoftRST_Deassert
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void MCR20Drv_SoftRST_Deassert
(
  void
);


/*---------------------------------------------------------------------------
 * Name: MCR20Drv_RESET
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void MCR20Drv_RESET
(
  void
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_Soft_RESET
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void MCR20Drv_Soft_RESET
(
  void
);

/*---------------------------------------------------------------------------
 * Name: MCR20Drv_Set_CLK_OUT_Freq
 * Description: -
 * Parameters: -
 * Return: -
 *---------------------------------------------------------------------------*/
void MCR20Drv_Set_CLK_OUT_Freq
(
  uint8_t freqDiv
);

/**************************************************************************************************/
/*                                        XCVR GPIO API                                           */
/**************************************************************************************************/

/*! *********************************************************************************
* \brief  Configure the XCVR GPIO pin as output pin
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_SetGpioPinAsOutput(uint8_t GpioMask);

/*! *********************************************************************************
* \brief  Configure the XCVR GPIO pin as input pin
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_SetGpioPinAsInput(uint8_t GpioMask);

/*! *********************************************************************************
* \brief  Set the XCVR GPIO pin value to logic 1 (one)
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
* \remarks Use only if the pin is set up as output
*
********************************************************************************** */
void MCR20Drv_SetGpioPin(uint8_t GpioMask);

/*! *********************************************************************************
* \brief  Set the XCVR GPIO pin value to logic 0 (zero)
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
* \remarks Use only if the pin is set up as output
*
********************************************************************************** */
void MCR20Drv_ClearGpioPin(uint8_t GpioMask);

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
uint8_t MCR20Drv_GetGpioPinValue(uint8_t GpioMask);

/*! *********************************************************************************
* \brief  Enable XCVR GPIO pin PullUp resistor
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_EnableGpioPullUp(uint8_t GpioMask);

/*! *********************************************************************************
* \brief  Enable XCVR GPIO pin PullDown resistor
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_EnableGpioPullDown(uint8_t GpioMask);

/*! *********************************************************************************
* \brief  Disable XCVR GPIO pin PullUp/PullDown resistors
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_DisableGpioPullUpDown(uint8_t GpioMask);

/*! *********************************************************************************
* \brief  Enable hi drive strength on XCVR GPIO pin
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_EnableGpioHiDriveStrength(uint8_t GpioMask);

/*! *********************************************************************************
* \brief  Disable hi drive strength on XCVR GPIO pin
*
* \param[in]  GpioMask  bitmask of GPIOs: bit0 -> GPIO1, ..., bit7 -> GPIO8
*
********************************************************************************** */
void MCR20Drv_DisableGpioHiDriveStrength(uint8_t GpioMask);

#endif /* __MCR20_DRV_H__ */
