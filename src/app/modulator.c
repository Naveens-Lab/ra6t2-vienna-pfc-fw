/***********************************************************************************************************************//**
 * @file    modulator.c
 * @brief   Space-vector PWM via min/max zero-sequence injection (Task 5) — pure function.
 *
 * Method:
 *   1. Inverse Clarke (Valpha,Vbeta) -> phase-to-neutral references (va,vb,vc).
 *   2. Zero-sequence injection: vsn = -(max(v) + min(v)) / 2. Added to every phase, this is the
 *      common-mode that turns sine references into SVPWM — it does not change any line-to-line
 *      voltage (so it is invisible to current control) but extends the linear range to Vdc/sqrt(3),
 *      ~15.5% more bus utilisation than sine PWM. The result is the classic saddle-shaped duty.
 *   3. Map to duty: d_x = 0.5 + (v_x + vsn)/Vdc.
 *   4. Neutral-balance offset: another common-mode term added to all three duties. Like vsn it is
 *      invisible to line-to-line voltage, but it biases midpoint conduction to correct split-bus drift.
 *   5. Clamp to [0,1].
 *
 * At the linear-modulation limit (peak phase voltage = Vdc/sqrt(3)) the duties just reach 0 and 1 at
 * the sector boundaries. Pure / HAL-free -> host-testable.
 **************************************************************************************************************************/
#include "modulator.h"
#include "transforms.h"
#include "pfc_config.h"

/*! Below this bus voltage, normalisation is skipped (output centred) to avoid divide-by-zero. */
#define MOD_VDC_EPS   (1.0e-3f)

static inline float mod_max3 (float a, float b, float c)
{
    float m = (a > b) ? a : b;
    return (m > c) ? m : c;
}

static inline float mod_min3 (float a, float b, float c)
{
    float m = (a < b) ? a : b;
    return (m < c) ? m : c;
}

static inline float mod_clamp_duty (float duty)
{
    if (duty < PFC_DUTY_MIN)
    {
        return PFC_DUTY_MIN;
    }
    if (duty > PFC_DUTY_MAX)
    {
        return PFC_DUTY_MAX;
    }
    return duty;
}

pfc_duty_t svpwm (float v_alpha, float v_beta, float v_dc, float balance_offset)
{
    /* 1. Stationary-frame command -> phase-to-neutral references. */
    const pfc_ab_t  ab = { v_alpha, v_beta };
    const pfc_abc_t v  = inv_clarke_ab_to_abc(ab);

    /* 2. Min/max zero-sequence injection (common-mode, cancels in line-to-line). */
    const float vsn = -0.5f * (mod_max3(v.a, v.b, v.c) + mod_min3(v.a, v.b, v.c));

    /* 3. Normalise to duty. Guard a collapsed bus. */
    const float inv_vdc = (v_dc > MOD_VDC_EPS) ? (1.0f / v_dc) : 0.0f;

    /* 3+4. Map to duty and add the neutral-balance common-mode offset. */
    pfc_duty_t required_duty;
    required_duty.da = 0.5f + ((v.a + vsn) * inv_vdc) + balance_offset;
    required_duty.db = 0.5f + ((v.b + vsn) * inv_vdc) + balance_offset;
    required_duty.dc = 0.5f + ((v.c + vsn) * inv_vdc) + balance_offset;

    /* 5. Clamp to the valid duty range. */
    required_duty.da = mod_clamp_duty(required_duty.da);
    required_duty.db = mod_clamp_duty(required_duty.db);
    required_duty.dc = mod_clamp_duty(required_duty.dc);

    return required_duty;
}
