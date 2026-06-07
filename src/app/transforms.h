/***********************************************************************************************************************//**
 * @file    transforms.h
 * @brief   Clarke / Park coordinate transforms (pure functions).
 *
 * The dq control strategy hinges on these: Clarke collapses 3 phases into one stationary vector,
 * Park rotates it into a frame spinning with the grid so the AC quantities become DC and are
 * PI-controllable. All functions are pure (inputs/outputs by value, no global state) and therefore
 * unit-testable on a host — a hard requirement, since correctness here is non-negotiable.
 **************************************************************************************************************************/
#ifndef TRANSFORMS_H_
#define TRANSFORMS_H_

#include "pfc_types.h"

/*!
 * @brief  Clarke transform: abc (natural) -> alpha-beta (stationary), amplitude-invariant form.
 * @param  in  three-phase quantity (a,b,c)
 * @return stationary-frame vector (alpha,beta)
 */
pfc_ab_t  clarke_abc_to_ab(pfc_abc_t in);

/*!
 * @brief  Inverse Clarke transform: alpha-beta -> abc (zero-sequence assumed zero).
 * @param  in  stationary-frame vector (alpha,beta)
 * @return three-phase quantity (a,b,c)
 */
pfc_abc_t inv_clarke_ab_to_abc(pfc_ab_t in);

/*!
 * @brief  Park transform: alpha-beta (stationary) -> dq (synchronous frame).
 * @param  in     stationary-frame vector (alpha,beta)
 * @param  theta  rotation angle [rad]
 * @return rotating-frame vector (d,q)
 */
pfc_dq_t  park_ab_to_dq(pfc_ab_t in, float theta);

/*!
 * @brief  Inverse Park transform: dq -> alpha-beta. Exact inverse of ::park_ab_to_dq.
 * @param  in     rotating-frame vector (d,q)
 * @param  theta  rotation angle [rad]
 * @return stationary-frame vector (alpha,beta)
 */
pfc_ab_t  inv_park_dq_to_ab(pfc_dq_t in, float theta);

#endif /* TRANSFORMS_H_ */
