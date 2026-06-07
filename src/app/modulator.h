/***********************************************************************************************************************//**
 * @file    modulator.h
 * @brief   Space-vector PWM (SVPWM) modulator (pure function).
 *
 * Maps the commanded stationary-frame voltage vector (Valpha, Vbeta) to three per-phase duties
 * using min/max zero-sequence injection (~15% more DC-bus utilisation than sine PWM, for free).
 * The neutral-balance offset is applied here, and duties are clamped to [0,1].
 **************************************************************************************************************************/
#ifndef MODULATOR_H_
#define MODULATOR_H_

#include "pfc_types.h"

/*!
 * Compute three-phase duties from a reference voltage vector.
 * @param v_alpha         stationary-frame alpha voltage command [V]
 * @param v_beta          stationary-frame beta voltage command [V]
 * @param v_dc            measured total DC-bus voltage [V] (for normalisation)
 * @param balance_offset  neutral-point balance correction added to all phases [normalised]
 * @return per-phase duties in [0,1]
 */
pfc_duty_t svpwm(float v_alpha, float v_beta, float v_dc, float balance_offset);

#endif /* MODULATOR_H_ */
