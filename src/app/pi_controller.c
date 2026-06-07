/***********************************************************************************************************************//**
 * @file    pi_controller.c
 * @brief   Generic discrete PI controller with back-calculation anti-windup (Task 4).
 *
 * Discrete form (forward Euler), matching the continuous law  u = Kp*e + I,  dI/dt = Ki*e + Kaw*(u_sat - u):
 *     u     = Kp*e + I                       (pre-saturation output)
 *     u_sat = clamp(u, out_min, out_max)
 *     I    += dt*( Ki*e + Kaw*(u_sat - u) )  (integrate; back-calc bleeds the integrator when clamped)
 *     out   = u_sat
 *
 * When unsaturated, (u_sat - u) = 0 and the integrator behaves normally. When the output is clamped,
 * the Kaw term drives the integrator back so it cannot wind up — essential during pre-charge / ramp /
 * transient saturation. Pure: all state in the caller-owned ::pi_controller_t, so it is host-testable.
 **************************************************************************************************************************/
#include <stddef.h>
#include "pi_controller.h"

static inline float pi_clampf (float v, float lo, float hi)
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

void pi_reset (pi_controller_t * p_pi)
{
    if (NULL != p_pi)
    {
        p_pi->integrator = 0.0f;
    }
}

float pi_step (pi_controller_t * p_pi, const pi_params_t * p_params, float error)
{
    /* Pre-saturation output: proportional path + integral state. */
    const float unsat = (p_params->kp * error) + p_pi->integrator;

    /* Saturate to the actuator limits. */
    const float sat = pi_clampf(unsat, p_params->out_min, p_params->out_max);

    /* Integrate with back-calculation anti-windup. */
    p_pi->integrator += ((p_params->ki * error) + (p_params->kaw * (sat - unsat))) * p_params->dt;

    return sat;
}
