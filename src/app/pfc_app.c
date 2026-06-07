/***********************************************************************************************************************//**
 * @file    pfc_app.c
 * @brief   Application supervisor: startup sequencing and state machine.
 *
 * Owns the safe power-up order so the converter is never energised out of sequence:
 *
 *   INIT ──► PRECHARGE ──► PLL_LOCK ──► RAMP ──► RUN
 *     │          │            │          │        │
 *     └──────────┴────────────┴──────────┴────────┴────────►  FAULT  (latched, gates off)
 *
 *  - INIT      : one-time peripheral bring-up (ADC + HW cal, protection arm, control/timer open),
 *                start PWM in IDLE (gates switch at 50% = no current shaping; this is the open-loop
 *                state), then measure the bipolar current offsets at zero current.
 *  - PRECHARGE : wait for the bus to charge through the external current-limited path (monitor V_bus).
 *  - PLL_LOCK  : wait for grid synchronisation (theta valid) before any switching authority.
 *  - RAMP      : soft-start — the control loop ramps the Id_ref ceiling up gently.
 *  - RUN       : full closed-loop regulation.
 *  - FAULT     : latched safe state — PWM stopped, gates forced off, loops idle.
 *
 * The fast control loop runs in the ADC ISR (pfc_control); this supervisor runs in the background
 * main loop and is non-blocking except for the one-time INIT. Timing uses the control-loop tick
 * counter (PWM-rate) as a wall-clock base.
 **************************************************************************************************************************/
#include "pfc_app.h"
#include "pfc_config.h"
#include "adc_manager.h"
#include "protection.h"
#include "pfc_control.h"

/*! Supervisor state. Shared with the ISR path (fault transitions) -> volatile. */
static volatile pfc_state_t global_state = PFC_STATE_INIT;

/*! Control-tick stamp of the current state's entry, for timeout measurement. */
static uint32_t global_state_entry_ticks = 0U;


static void state_enter (pfc_state_t state)
{
    global_state_entry_ticks = pfc_control_ticks();
    global_state             = state;
}

static uint32_t state_elapsed_cycles (void)
{
    return pfc_control_ticks() - global_state_entry_ticks;
}

/*! Transition to the latched FAULT state and make the stage safe */
static void go_fault (void)
{
    pfc_control_set_mode(PFC_CTRL_MODE_IDLE);
    pfc_control_stop();          /* GPT stop -> gate pins to stop level Low */
    protection_force_disable();  /* force POEG output-disable as a hardware backstop */
    state_enter(PFC_STATE_FAULT);
}

/*--------------------------------------------------------------------------------------------------------------------*
 * Public API
 *--------------------------------------------------------------------------------------------------------------------*/

void pfc_app_init (void)
{
    global_state = PFC_STATE_INIT;

    /* One-time peripheral bring-up, in safe order. Each *_init safe-halts internally on HW failure. */
    adc_manager_init();        /* open + HW calibrate ADC, arm GPT trigger          */
    protection_init();         /* DAC threshold + ACMPHS + POEG armed (live first)  */
    pfc_control_init();        /* open 3-phase timer, preload 50% duties, init loops */

    /* Begin sensing/PLL with no current shaping, then capture the zero-current offsets. */
    pfc_control_set_mode(PFC_CTRL_MODE_IDLE);
    pfc_control_start();               /* GPT runs -> ADC scans -> control ISR (IDLE) */
    adc_manager_measure_offsets();     /* average N scans at zero current (required)   */

    state_enter(PFC_STATE_PRECHARGE);
}

void pfc_app_run (void)
{
    /* Hardware OCP latch (POEG) takes precedence from any state. */
    if (protection_is_faulted() && (PFC_STATE_FAULT != global_state))
    {
        go_fault();
        return;
    }

    const pfc_measurements_t * current_adc_val = adc_manager_latest();

    /* Software over-voltage guard in any energised state. */
    if ((PFC_STATE_PRECHARGE == global_state) || (PFC_STATE_PLL_LOCK == global_state) ||
        (PFC_STATE_RAMP == global_state) || (PFC_STATE_RUN == global_state))
    {
        if (current_adc_val->v_bus > PFC_VBUS_OV_V)
        {
            go_fault();
            return;
        }
    }

    switch (global_state)
    {
        case PFC_STATE_PRECHARGE:
        {
            if (current_adc_val->v_bus >= PFC_PRECHARGE_DONE_V)
            {
                state_enter(PFC_STATE_PLL_LOCK);
            }
            else if (state_elapsed_cycles() > PFC_SECONDS_TO_CYCLES(PFC_PRECHARGE_TIMEOUT_S))
            {
                go_fault();   /* bus failed to charge */
            }
            break;
        }

        case PFC_STATE_PLL_LOCK:
        {
            if (pfc_control_is_pll_locked())
            {
                pfc_control_set_mode(PFC_CTRL_MODE_RAMP);
                state_enter(PFC_STATE_RAMP);
            }
            else if (state_elapsed_cycles() > PFC_SECONDS_TO_CYCLES(PFC_PLL_LOCK_TIMEOUT_S))
            {
                go_fault();   /* failed to synchronise */
            }
            break;
        }

        case PFC_STATE_RAMP:
        {
            if (pfc_control_is_ramp_done())
            {
                pfc_control_set_mode(PFC_CTRL_MODE_RUN);
                state_enter(PFC_STATE_RUN);
            }
            break;
        }

        case PFC_STATE_RUN:
        {
            /* Steady-state regulation handled entirely in the control ISR; supervisor just monitors. */
            break;
        }

        case PFC_STATE_FAULT:
        {
            /* Latched: remain safe. Recovery requires a reset or power cycle. */
            break;
        }

        case PFC_STATE_INIT:
        default:
        {
            /* INIT work is done in pfc_app_init(); should not be reached at run time. */
            break;
        }
    }
}

pfc_state_t pfc_app_state (void)
{
    return global_state;
}
