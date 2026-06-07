/***********************************************************************************************************************//**
 * @file    pll.c
 * @brief   Synchronous-reference-frame PLL— locks theta/omega to the grid.
 *
 * Per control cycle: Clarke+Park the grid voltages with the current angle estimate, take the q-axis
 * as the phase-error signal (q = |V| sin(theta_grid - theta) -> 0 at lock), NORMALISE it by the
 * vector magnitude so loop gain is amplitude-independent, run a PI loop filter to produce a
 * frequency correction, and integrate omega into theta (the VCO). Lock asserts after the normalised
 * error stays inside a band for a dwell time.
 *
 * Pure with respect to globals: all state lives in the caller-owned ::pll_t, so this is host-testable.
 **************************************************************************************************************************/
#include <math.h>
#include <stddef.h>
#include "pll.h"
#include "pfc_config.h"
#include "transforms.h"

/*! Clamp helper (single-precision). */
static inline float pll_clampf (float v, float lo, float hi)
{
    if (v < lo)
    {
        return lo;
    }
    if (v > hi)
    {
        return hi;
    }
    return v;
}

void pll_init (pll_t * p_pll)
{
    if (NULL != p_pll)
    {
        p_pll->theta      = 0.0f;
        p_pll->omega      = PFC_GRID_OMEGA_NOMINAL;
        p_pll->integrator = 0.0f;
        p_pll->lock_count = 0U;
        p_pll->locked     = false;
    }
}

pll_output_t pll_step (pll_t * p_pll, pfc_abc_t v_grid)
{
    /* 1. Grid voltages -> stationary -> rotating frame at the current angle estimate. */
    const pfc_ab_t ab = clarke_abc_to_ab(v_grid);
    const pfc_dq_t dq = park_ab_to_dq(ab, p_pll->theta);

    /* 2. Normalised phase-error: err = q / |V| ~= sin(theta_grid - theta). Amplitude-independent;
     *    guarded against a dead grid (|V| ~ 0) so we don't divide by zero or chase noise. */
    const float mag = sqrtf((dq.d * dq.d) + (dq.q * dq.q));
    float err = 0.0f;
    if (mag > PFC_PLL_MAG_EPS)
    {
        err = dq.q / mag;
    }

    /* 3. PI loop filter -> frequency correction (Δω). Integrator clamped to the allowed frequency
     *    band: this is the VCO anti-windup and keeps the estimate from running away off-grid. */
    p_pll->integrator += PFC_PLL_KI * err * PFC_CTRL_PERIOD_S;
    p_pll->integrator  = pll_clampf(p_pll->integrator, PFC_PLL_DOMEGA_MIN, PFC_PLL_DOMEGA_MAX);

    const float delta_omega = (PFC_PLL_KP * err) + p_pll->integrator;
    p_pll->omega = pll_clampf(PFC_GRID_OMEGA_NOMINAL + delta_omega, PFC_PLL_OMEGA_MIN, PFC_PLL_OMEGA_MAX);

    /* 4. VCO: integrate omega into theta, wrap to [0, 2*pi). */
    p_pll->theta += p_pll->omega * PFC_CTRL_PERIOD_S;
    while (p_pll->theta >= PFC_TWO_PI)
    {
        p_pll->theta -= PFC_TWO_PI;
    }
    while (p_pll->theta < 0.0f)
    {
        p_pll->theta += PFC_TWO_PI;
    }

    /* 5. Lock detection: normalised error inside the band for a dwell count -> locked. */
    if (fabsf(err) < PFC_PLL_LOCK_ERR_THRESH)
    {
        if (p_pll->lock_count < PFC_PLL_LOCK_DWELL_CYCLES)
        {
            p_pll->lock_count++;
        }
    }
    else
    {
        p_pll->lock_count = 0U;
    }
    p_pll->locked = (p_pll->lock_count >= PFC_PLL_LOCK_DWELL_CYCLES);

    pll_output_t out = { p_pll->theta, p_pll->omega, p_pll->locked };
    return out;
}
