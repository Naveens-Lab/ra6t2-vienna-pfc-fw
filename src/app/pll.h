/***********************************************************************************************************************//**
 * @file    pll.h
 * @brief   Synchronous-reference-frame PLL (SRF-PLL) for grid synchronisation.
 *
 * Locks an internal angle theta to the grid by Clarke+Park-ing the 3-phase voltages and driving
 * Vq -> 0 with a PI on a VCO integrator. Theta is the foundation of the whole control scheme:
 * every Park transform downstream depends on it, so nothing is valid until the PLL locks.
 * Pure with respect to globals — state lives in a caller-owned ::pll_t.
 **************************************************************************************************************************/
#ifndef PLL_H_
#define PLL_H_

#include "pfc_types.h"

/*! SRF-PLL run-time state. */
typedef struct
{
    float theta;          /*!< Current angle estimate [rad]                         */
    float omega;          /*!< Current frequency estimate [rad/s]                   */
    float integrator;     /*!< Loop-filter (PI) integral term                       */
    uint32_t lock_count;  /*!< Consecutive in-tolerance cycles (lock hysteresis)    */
    bool  locked;         /*!< Lock status                                          */
} pll_t;

/*! Initialise the PLL to nominal frequency, zero angle, unlocked. */
void pll_init(pll_t * p_pll);

/*!
 * Advance the PLL one control cycle.
 * @param p_pll    PLL state (updated in place)
 * @param v_grid   measured 3-phase grid voltages [V]
 * @return angle/frequency/lock snapshot for this cycle
 */
pll_output_t pll_step(pll_t * p_pll, pfc_abc_t v_grid);

#endif /* PLL_H_ */
