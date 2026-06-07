/***********************************************************************************************************************//**
 * @file    pfc_control.h
 * @brief   The control-loop orchestration — the heart of the firmware.
 *
 * pfc_control_isr() runs once per PWM cycle on the ADC trough trigger (invoked from the ADC
 * scan-complete path). It chains:
 *   fault-guard -> snapshot -> PLL -> Clarke -> Park -> PI loops -> inv-Park -> SVPWM -> duty write,
 * with the slow (voltage / neutral-balance) loops run on a cycle divider. It must complete within
 * the per-cycle budget and never block.
 **************************************************************************************************************************/
#ifndef PFC_CONTROL_H_
#define PFC_CONTROL_H_

#include <stdbool.h>
#include <stdint.h>

/*! Control-loop actuation mode, set by the supervisor (pfc_app) to gate how much the ISR drives. */
typedef enum
{
    PFC_CTRL_MODE_IDLE = 0,  /*!< sense + PLL only; zero modulation (50% duties), no current shaping */
    PFC_CTRL_MODE_RAMP,      /*!< current loops active; Id_ref ceiling ramps up gently (soft-start)   */
    PFC_CTRL_MODE_RUN        /*!< full closed-loop regulation                                         */
} pfc_control_mode_t;

/*! Open the 3-phase PWM timer, set safe (50%) duties, and initialise control state (PLL, PI, refs).
 *  Call once before starting PWM. Safe-halts on timer-open failure. */
void pfc_control_init(void);

/*! Start the 3-phase PWM timer (begins gate switching AND the ADC trough trigger / control loop). */
void pfc_control_start(void);

/*! Stop the 3-phase PWM timer (halts switching and the control-loop trigger). */
void pfc_control_stop(void);

/*! One control-loop iteration. Invoked from the ADC scan-complete path; deterministic, non-blocking. */
void pfc_control_isr(void);

/*! Set the actuation mode (supervisor -> ISR). Entering RAMP restarts the Id_ref soft-start ramp. */
void pfc_control_set_mode(pfc_control_mode_t mode);

/*! @return true once the SRF-PLL has locked to the grid (for the supervisor state machine). */
bool pfc_control_is_pll_locked(void);

/*! @return true once the RAMP soft-start has reached the full Id_ref ceiling. */
bool pfc_control_is_ramp_done(void);

/*! @return free-running control-loop tick count (one per ISR @ PWM rate) — the supervisor's time base. */
uint32_t pfc_control_ticks(void);

#endif /* PFC_CONTROL_H_ */
