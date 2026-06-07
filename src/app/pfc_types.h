/***********************************************************************************************************************//**
 * @file    pfc_types.h
 * @brief   Shared value types for the Vienna PFC control chain.
 *
 * These are small, copyable, by-value structs that flow between the (pure) control-math modules:
 *   abc (three-phase)  ->  alpha-beta (Clarke)  ->  dq (Park)  ->  back out  ->  duties.
 * Keeping them here lets transforms/PLL/PI/modulator stay free of any HAL dependency and be
 * unit-tested on a host.
 **************************************************************************************************************************/
#ifndef PFC_TYPES_H_
#define PFC_TYPES_H_

#include <stdint.h>
#include <stdbool.h>

/*! Three-phase quantity in the natural (abc) reference frame. */
typedef struct
{
    float a;
    float b;
    float c;
} pfc_abc_t;

/*! Two-phase quantity in the stationary (alpha-beta) frame (Clarke output). */
typedef struct
{
    float alpha;
    float beta;
} pfc_ab_t;

/*! Two-phase quantity in the synchronous rotating (d-q) frame (Park output). */
typedef struct
{
    float d;
    float q;
} pfc_dq_t;

/*! Per-phase PWM duty cycles, normalised to [0,1] (U, V, W). */
typedef struct
{
    float da;
    float db;
    float dc;
} pfc_duty_t;

/*! SRF-PLL output: grid angle/speed plus a lock flag for the supervisor. */
typedef struct
{
    float theta;    /*!< Estimated grid angle [rad], wrapped to [0, 2*pi)      */
    float omega;    /*!< Estimated grid angular frequency [rad/s]              */
    bool  locked;   /*!< Asserted once the PLL has tracked within tolerance    */
} pll_output_t;

/*!
 * Calibrated sensor snapshot in engineering units — the single, atomic boundary the
 * control loop reads (see adc_manager). Derived fields are computed once by the manager
 * so downstream code never re-derives them.
 */
typedef struct
{
    float va;        /*!< Grid phase voltage A [V]                 */
    float vb;        /*!< Grid phase voltage B [V]                 */
    float vc;        /*!< Grid phase voltage C [V]                 */
    float ia;        /*!< Inductor current A [A]                   */
    float ib;        /*!< Inductor current B [A]                   */
    float ic;        /*!< Inductor current C [A]                   */
    float v_pos;     /*!< Upper bus rail (+) [V]                   */
    float v_neg;     /*!< Lower bus rail (-) [V]                   */
    float v_bus;     /*!< Derived: v_pos + v_neg (total bus) [V]   */
    float v_mid_err; /*!< Derived: v_pos - v_neg (neutral imbalance) [V] */
} pfc_measurements_t;

/*! Top-level supervisor states */
typedef enum
{
    PFC_STATE_INIT = 0,   /*!< Peripherals init, self-checks                                  */
    PFC_STATE_PRECHARGE,  /*!< Soft-charge the bus through the current-limited path           */
    PFC_STATE_PLL_LOCK,   /*!< Wait for grid synchronisation                                  */
    PFC_STATE_RAMP,       /*!< Soft-start: ramp Id_ref up gently                              */
    PFC_STATE_RUN,        /*!< Full closed-loop regulation                                    */
    PFC_STATE_FAULT       /*!< Latched fault: gates off, safe halt                            */
} pfc_state_t;

#endif /* PFC_TYPES_H_ */
