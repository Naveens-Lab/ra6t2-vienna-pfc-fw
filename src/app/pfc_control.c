/***********************************************************************************************************************//**
 * @file    pfc_control.c
 * @brief   Control-loop assembly — the heart of the firmware.
 *
 * pfc_control_isr() executes once per PWM cycle, invoked from the ADC scan-complete path (the trough
 * trigger). The chain:
 *   fault-guard -> snapshot -> PLL -> Clarke(I) -> Park(I) -> [slow loops on divider] -> inner PI ->
 *   inv-Park -> SVPWM -> R_GPT_THREE_PHASE_DutyCycleSet.
 * The outer voltage loop (Vbus -> Id_ref) and the neutral-balance loop run every PFC_SLOW_LOOP_DIVIDER
 * cycles because those dynamics are slow; running them every cycle wastes ISR time and can destabilise.
 *
 * Determinism: no loops of unbounded length, no blocking calls, single-precision throughout.
 **************************************************************************************************************************/
#include "pfc_control.h"
#include "pfc_config.h"
#include "pfc_types.h"
#include "adc_manager.h"
#include "transforms.h"
#include "pll.h"
#include "pi_controller.h"
#include "modulator.h"
#include "protection.h"
#include "pfc_safe.h"
#include "hal_data.h"

/* Control-state instances */
static pll_t           global_pll;
static pi_controller_t global_pi_id;
static pi_controller_t global_pi_iq;
static pi_controller_t global_pi_vbus;
static pi_controller_t global_pi_balance;
static uint32_t        global_slow_loop_counter = 0U;

/* Cross-cycle control state. */
static float           g_id_ref         = 0.0f;   /*!< active-current reference from the voltage loop  */
static float           g_balance_offset = 0.0f;   /*!< neutral-balance common-mode duty offset         */
static float           g_id_ramp_limit  = 0.0f;   /*!< soft-start ceiling on |Id_ref| (rises in RAMP)  */
static uint32_t        g_period_counts  = 1U;     /*!< cached PWM period [timer counts] (GTPR)          */
static volatile bool   g_pll_locked     = false;  /*!< latest PLL lock status (read by supervisor)      */
static volatile bool   g_ramp_done      = false;  /*!< RAMP reached full ceiling                        */
static volatile uint32_t g_control_ticks = 0U;    /*!< free-running ISR tick counter (supervisor clock) */
static volatile pfc_control_mode_t g_mode = PFC_CTRL_MODE_IDLE;  /*!< actuation mode (set by supervisor)*/

/*--------------------------------------------------------------------------------------------------------------------*
 * Loop parameter sets Gains are placeholders in pfc_config.h pending plant identification;
 * the structure, clamps, anti-windup gain, and sample periods are final. Inner loops run every cycle
 * (dt = control period); the outer voltage and neutral-balance loops run on the slow-loop divider
 * (dt = slow-loop period) so their integral action is timed correctly.
 *--------------------------------------------------------------------------------------------------------------------*/

/* Inner current loop, d-axis: Id error -> Vd command. */
static const pi_params_t PI_PARAMS_ID =
{
    .kp = PFC_PI_ID_KP, .ki = PFC_PI_ID_KI,
    .out_min = -PFC_VD_MAX, .out_max = PFC_VD_MAX,
    .kaw = PFC_PI_ANTIWINDUP_GAIN, .dt = PFC_CTRL_PERIOD_S
};

/* Inner current loop, q-axis: Iq error (ref 0, unity PF) -> Vq command. */
static const pi_params_t PI_PARAMS_IQ =
{
    .kp = PFC_PI_IQ_KP, .ki = PFC_PI_IQ_KI,
    .out_min = -PFC_VQ_MAX, .out_max = PFC_VQ_MAX,
    .kaw = PFC_PI_ANTIWINDUP_GAIN, .dt = PFC_CTRL_PERIOD_S
};

/* Outer voltage loop (slow): Vbus error -> Id_ref. Rectifier draws power, so Id_ref >= 0. */
static const pi_params_t PI_PARAMS_VBUS =
{
    .kp = PFC_PI_VBUS_KP, .ki = PFC_PI_VBUS_KI,
    .out_min = 0.0f, .out_max = PFC_ID_REF_MAX_A,
    .kaw = PFC_PI_ANTIWINDUP_GAIN, .dt = PFC_SLOW_LOOP_PERIOD_S
};

/* Neutral-balance loop (slow): (V+ - V-) -> symmetric duty offset. */
static const pi_params_t PI_PARAMS_BALANCE =
{
    .kp = PFC_PI_BAL_KP, .ki = PFC_PI_BAL_KI,
    .out_min = -PFC_BALANCE_OFFSET_MAX, .out_max = PFC_BALANCE_OFFSET_MAX,
    .kaw = PFC_PI_ANTIWINDUP_GAIN, .dt = PFC_SLOW_LOOP_PERIOD_S
};

/* Per-loop step wrappers. Kept static inline so they bind state to params without unused-symbol warnings. */
static inline float loop_id_step (float id_err)
{
    return pi_step(&global_pi_id, &PI_PARAMS_ID, id_err);
}

static inline float loop_iq_step (float iq_err)
{
    return pi_step(&global_pi_iq, &PI_PARAMS_IQ, iq_err);
}

static inline float loop_vbus_step (float vbus_err)
{
    return pi_step(&global_pi_vbus, &PI_PARAMS_VBUS, vbus_err);
}

static inline float loop_balance_step (float balance_err)
{
    return pi_step(&global_pi_balance, &PI_PARAMS_BALANCE, balance_err);
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*! Convert a normalised duty [0,1] to a GPT compare count, clamped to the legal (0, GTPR) range.
 *  In triangle-symmetric mode GTPR == period_counts and count == d * period_counts. */
static inline uint32_t duty_to_counts (float duty)
{
    int32_t counts = (int32_t) ((duty * (float) g_period_counts) + 0.5f);
    if (counts < 1)
    {
        counts = 1;
    }
    else if (counts > (int32_t) (g_period_counts - 1U))
    {
        counts = (int32_t) (g_period_counts - 1U);
    }
    return (uint32_t) counts;
}

/*! Write the three phase duties (normalised) to the GPT, applied on the next cycle (buffered). */
static inline void write_duties (pfc_duty_t duty)
{
    three_phase_duty_cycle_t dc;
    dc.duty[THREE_PHASE_CHANNEL_U] = duty_to_counts(duty.da);
    dc.duty[THREE_PHASE_CHANNEL_V] = duty_to_counts(duty.db);
    dc.duty[THREE_PHASE_CHANNEL_W] = duty_to_counts(duty.dc);
    /* Single-buffer mode: duty_buffer is unused, but mirror it for safety/portability. */
    dc.duty_buffer[THREE_PHASE_CHANNEL_U] = dc.duty[THREE_PHASE_CHANNEL_U];
    dc.duty_buffer[THREE_PHASE_CHANNEL_V] = dc.duty[THREE_PHASE_CHANNEL_V];
    dc.duty_buffer[THREE_PHASE_CHANNEL_W] = dc.duty[THREE_PHASE_CHANNEL_W];
    (void) R_GPT_THREE_PHASE_DutyCycleSet(&g_three_phase0_ctrl, &dc);
}

/*! Reset all integrating control state to a clean start (used by init and on fault).
 *  Does NOT touch the free-running tick counter or the supervisor-owned mode. */
static void pfc_control_reset_state (void)
{
    pll_init(&global_pll);
    pi_reset(&global_pi_id);
    pi_reset(&global_pi_iq);
    pi_reset(&global_pi_vbus);
    pi_reset(&global_pi_balance);
    global_slow_loop_counter = 0U;
    g_id_ref            = 0.0f;
    g_balance_offset    = 0.0f;
    g_id_ramp_limit     = 0.0f;
    g_ramp_done         = false;
    g_pll_locked        = false;
}

/*! Reset only the current/voltage/balance loop integrators + references (PLL keeps tracking).
 *  Used while in IDLE so the loops are primed at zero and cannot wind up before actuation. */
static inline void pfc_control_hold_loops (void)
{
    pi_reset(&global_pi_id);
    pi_reset(&global_pi_iq);
    pi_reset(&global_pi_vbus);
    pi_reset(&global_pi_balance);
    global_slow_loop_counter = 0U;
    g_id_ref            = 0.0f;
    g_balance_offset    = 0.0f;
    g_id_ramp_limit     = 0.0f;
    g_ramp_done         = false;
}

void pfc_control_init (void)
{
    /* Open the 3-phase PWM timer so duties can be written (it is started later, by the supervisor). */
    fsp_err_t err = R_GPT_THREE_PHASE_Open(&g_three_phase0_ctrl, &g_three_phase0_cfg);
    if (FSP_SUCCESS != err)
    {
        pfc_safe_halt();
    }

    /* Cache the PWM period (GTPR == period_counts in triangle-symmetric mode). */
    g_period_counts = g_timer0_cfg.period_counts;

    pfc_control_reset_state();

    /* Pre-load safe, centred (50%) duties before any switching begins. */
    pfc_duty_t centred = { 0.5f, 0.5f, 0.5f };
    write_duties(centred);
}

void pfc_control_start (void)
{
    (void) R_GPT_THREE_PHASE_Start(&g_three_phase0_ctrl);
}

void pfc_control_stop (void)
{
    (void) R_GPT_THREE_PHASE_Stop(&g_three_phase0_ctrl);
}

void pfc_control_set_mode (pfc_control_mode_t mode)
{
    /* Entering RAMP restarts the soft-start ceiling from zero. */
    if (PFC_CTRL_MODE_RAMP == mode)
    {
        g_id_ramp_limit = 0.0f;
        g_ramp_done     = false;
    }
    g_mode = mode;
}

bool pfc_control_is_pll_locked (void)
{
    return g_pll_locked;
}

bool pfc_control_is_ramp_done (void)
{
    return g_ramp_done;
}

uint32_t pfc_control_ticks (void)
{
    return g_control_ticks;
}

/*the main ISR Which handles Vinenna Rectifier states*/
void pfc_control_isr (void)
{
    g_control_ticks++;   /* free-running time base for the supervisor (advances in every mode) */

    /* 1. Fault guard. POEG has already cut the gates in hardware; keep integrators clean for any
     *    deliberate restart and do not touch the actuator. */
    if (protection_is_faulted())
    {
        pfc_control_reset_state();
        return;
    }

    /* 2. Atomic sensor snapshot in engineering units. */
    const pfc_measurements_t * m = adc_manager_latest();

    /* 3. Grid synchronisation — runs in every mode so theta is locked before we actuate. */
    const pfc_abc_t    v_abc = { m->va, m->vb, m->vc };
    const pll_output_t pll   = pll_step(&global_pll, v_abc);
    const float        theta = pll.theta;
    g_pll_locked = pll.locked;

    /* 4. IDLE: sense + sync only. Hold the loops primed at zero and command zero modulation
     *    (50% duties) — no active current shaping. This is also the open-loop bench state. */
    if (PFC_CTRL_MODE_IDLE == g_mode)
    {
        pfc_control_hold_loops();
        const pfc_duty_t centred = svpwm(0.0f, 0.0f, m->v_bus, 0.0f);
        write_duties(centred);
        return;
    }

    /* 5. Soft-start ceiling on Id_ref: rises gently in RAMP, full in RUN. */
    if (PFC_CTRL_MODE_RAMP == g_mode)
    {
        g_id_ramp_limit += PFC_ID_RAMP_A_PER_CYCLE;
        if (g_id_ramp_limit >= PFC_ID_REF_MAX_A)
        {
            g_id_ramp_limit = PFC_ID_REF_MAX_A;
            g_ramp_done     = true;
        }
    }
    else /* PFC_CTRL_MODE_RUN */
    {
        g_id_ramp_limit = PFC_ID_REF_MAX_A;
        g_ramp_done     = true;
    }

    /* 6. Measured currents into the synchronous frame: Id (active), Iq (reactive). */
    const pfc_abc_t i_abc = { m->ia, m->ib, m->ic };
    const pfc_ab_t  i_ab  = clarke_abc_to_ab(i_abc);
    const pfc_dq_t  i_dq  = park_ab_to_dq(i_ab, theta);

    /* 7. Slow loops on the cycle divider: outer voltage loop -> Id_ref, neutral-balance -> offset.
     *    Id_ref is capped by the soft-start ceiling; the voltage-loop integrator is held to the same
     *    ceiling so it cannot wind up past the ramp during soft-start. */
    global_slow_loop_counter++;
    if (global_slow_loop_counter >= PFC_SLOW_LOOP_DIVIDER)
    {
        global_slow_loop_counter = 0U;

        float id_ref = loop_vbus_step(PFC_VBUS_REF_VOLTS - m->v_bus);
        if (global_pi_vbus.integrator > g_id_ramp_limit)
        {
            global_pi_vbus.integrator = g_id_ramp_limit;
        }
        if (id_ref > g_id_ramp_limit)
        {
            id_ref = g_id_ramp_limit;
        }
        g_id_ref = id_ref;

        g_balance_offset = loop_balance_step(m->v_mid_err);   /* drive (V+ - V-) -> 0 */
    }

    /* 8. Inner current loops every cycle: Id tracks Id_ref, Iq tracks 0 (unity power factor). */
    const float vd = loop_id_step(g_id_ref - i_dq.d);
    const float vq = loop_iq_step(PFC_IQ_REF - i_dq.q);

    /* 9. Back to the stationary frame, SVPWM (+balance offset), write duties (applied next cycle). */
    const pfc_dq_t   v_dq  = { vd, vq };
    const pfc_ab_t   v_ab  = inv_park_dq_to_ab(v_dq, theta);
    const pfc_duty_t duty  = svpwm(v_ab.alpha, v_ab.beta, m->v_bus, g_balance_offset);
    write_duties(duty);
}
