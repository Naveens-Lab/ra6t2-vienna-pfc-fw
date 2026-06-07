/***********************************************************************************************************************//**
 * @file    transforms.c
 * @brief   Clarke / Park coordinate transforms (Task 2) — pure, single-precision, host-testable.
 *
 * Conventions (fixed here and relied on by the whole control chain):
 *  - Clarke is AMPLITUDE-INVARIANT (2/3 scaling): a balanced set of peak-A phase quantities maps
 *    to an alpha-beta vector of magnitude A. Zero-sequence is dropped, so the round trip
 *    inv_clarke(clarke(x)) reproduces x only for balanced (a+b+c = 0) inputs — exactly the case in
 *    this 3-wire system.
 *  - Park rotates by theta:  d aligns with the vector at theta, q leads by 90 deg.
 *    inv_park(park(x, th), th) == x exactly (verified algebraically).
 *
 * No global state, no HAL dependency — these compile and unit-test unchanged on a host.
 **************************************************************************************************************************/
#include <math.h>
#include "transforms.h"

/* Transform constants (pure mathematics of the abc/alpha-beta basis, not tunables -> kept local). */
static const float TWO_THIRDS   = 2.0f / 3.0f;        /*!< amplitude-invariant Clarke scaling        */
static const float ONE_HALF     = 0.5f;
static const float SQRT3_OVER_2 = 0.8660254037844386f; /*!< sqrt(3)/2                                 */
static const float ONE_OVER_SQRT3 = 0.5773502691896258f; /*!< 1/sqrt(3)                              */

/*--------------------------------------------------------------------------------------------------------------------*/
pfc_ab_t clarke_abc_to_ab (pfc_abc_t in)
{
    pfc_ab_t out;

    /* alpha = (2/3)(a - b/2 - c/2);  beta = (1/sqrt3)(b - c)  — general form, no balance assumption. */
    out.alpha = TWO_THIRDS * (in.a - (ONE_HALF * in.b) - (ONE_HALF * in.c));
    out.beta  = ONE_OVER_SQRT3 * (in.b - in.c);

    return out;
}

/*--------------------------------------------------------------------------------------------------------------------*/
pfc_abc_t inv_clarke_ab_to_abc (pfc_ab_t in)
{
    pfc_abc_t out;

    /* Inverse of the amplitude-invariant Clarke (zero-sequence assumed zero). */
    out.a = in.alpha;
    out.b = (-ONE_HALF * in.alpha) + (SQRT3_OVER_2 * in.beta);
    out.c = (-ONE_HALF * in.alpha) - (SQRT3_OVER_2 * in.beta);

    return out;
}

/*--------------------------------------------------------------------------------------------------------------------*/
pfc_dq_t park_ab_to_dq (pfc_ab_t in, float theta)
{
    const float s = sinf(theta);
    const float c = cosf(theta);

    pfc_dq_t out;

    /* d =  alpha*cos + beta*sin ;  q = -alpha*sin + beta*cos */
    out.d = (in.alpha * c) + (in.beta * s);
    out.q = (-in.alpha * s) + (in.beta * c);

    return out;
}

/*--------------------------------------------------------------------------------------------------------------------*/
pfc_ab_t inv_park_dq_to_ab (pfc_dq_t in, float theta)
{
    const float s = sinf(theta);
    const float c = cosf(theta);

    pfc_ab_t out;

    /* alpha = d*cos - q*sin ;  beta = d*sin + q*cos  (exact inverse of park_ab_to_dq) */
    out.alpha = (in.d * c) - (in.q * s);
    out.beta  = (in.d * s) + (in.q * c);

    return out;
}
