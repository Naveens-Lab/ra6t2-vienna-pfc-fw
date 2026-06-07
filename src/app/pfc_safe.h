/***********************************************************************************************************************//**
 * @file    pfc_safe.h
 * @brief   Shared safe-halt primitive for unrecoverable initialisation failures.
 *
 * Used by the bring-up code paths (ADC / protection / control init) when an FSP operation fails before
 * any switching has begun. Stopping the CPU here is safe: no gates are driven yet, and the POEG keeps
 * gate outputs at the disable level in hardware regardless of CPU state.
 **************************************************************************************************************************/
#ifndef PFC_SAFE_H_
#define PFC_SAFE_H_

#include "bsp_api.h"

/*******************************************************************************************************************//**
 * @brief   Enter a permanent safe halt: disable interrupts and spin forever.
 *
 * Call only from init paths where continuing is unsafe (a required peripheral failed to come up).
 * Never returns.
 **********************************************************************************************************************/
static inline void pfc_safe_halt (void)
{
    __disable_irq();
    for ( ; ; )
    {
        
    }
}

#endif /* PFC_SAFE_H_ */
