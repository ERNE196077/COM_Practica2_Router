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

#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "board.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
/* Configuration for enter VLPR mode. Core clock = 4MHz. */
const mcg_config_t g_defaultClockConfigVlpr = {
    .mcgMode = kMCG_ModeBLPI,            /* Work in BLPI mode. */
    .irclkEnableMode = kMCG_IrclkEnable, /* MCGIRCLK enable. */
    .ircs = kMCG_IrcFast,                /* Select IRC4M. */
    .fcrdiv = 0U,                        /* FCRDIV is 0. */
    
    .frdiv = 0U,
    .drs = kMCG_DrsLow,         /* Low frequency range. */
    .dmx32 = kMCG_Dmx32Default, /* DCO has a default range of 25%. */
    .oscsel = kMCG_OscselOsc,   /* Select OSC. */
    
    .pll0Config =
    {
        .enableMode = 0U, /* Don't eanble PLL. */
        .prdiv = 0U,
        .vdiv = 0U,
    }
};

/* Configuration for enter RUN mode. Core clock = 120MHz. */
const mcg_config_t g_defaultClockConfigRun = {
    .mcgMode = kMCG_ModePEE,             /* Work in PEE mode. */
    .irclkEnableMode = kMCG_IrclkEnable, /* MCGIRCLK enable. */
    .ircs = kMCG_IrcFast,                /* Select IRC32k. */
    .fcrdiv = 0U,                        /* FCRDIV is 0. */

    .frdiv = 7U,
    .drs = kMCG_DrsLow,         /* Low frequency range. */
    .dmx32 = kMCG_Dmx32Default, /* DCO has a default range of 25%. */
    .oscsel = kMCG_OscselOsc,   /* Select OSC. */

    .pll0Config =
    {
        .enableMode = 0U,
        .prdiv = 0x13U,
        .vdiv = 0x18U,
    }
};

/*******************************************************************************
 * Code
 ******************************************************************************/
uint32_t BOARD_GetUartClock(uint32_t instance)
{
    instance = instance; /* Remove compiler warnings */
    return CLOCK_GetFreq(kCLOCK_CoreSysClk);
}

uint32_t BOARD_GetFtmClock(uint32_t instance)
{
    instance = instance; /* Remove compiler warnings */
    return CLOCK_GetFreq(kCLOCK_BusClk);
}

uint32_t BOARD_GetSpiClock(uint32_t instance)
{
    instance = instance; /* Remove compiler warnings */
    return CLOCK_GetFreq(kCLOCK_BusClk);    
}

uint32_t BOARD_GetI2cClock(uint32_t instance)
{
    instance = instance; /* Remove compiler warnings */
    return CLOCK_GetFreq(kCLOCK_BusClk);    
}

void BOARD_EnterLowPowerCb(void)
{
    CLOCK_SetMcgConfig(&g_defaultClockConfigVlpr);
}

void BOARD_ExitLowPowerCb(void)
{
    CLOCK_SetMcgConfig(&g_defaultClockConfigRun);
}

/*******************************************************************************
 * EOF
 ******************************************************************************/

 
