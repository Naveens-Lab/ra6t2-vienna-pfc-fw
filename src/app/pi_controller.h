/***********************************************************************************************************************//**
 * @file    pi_controller.h
 * @brief   Generic discrete PI controller with output clamping and back-calculation anti-windup.
 *
 * Reused by every loop (inner current d/q, outer voltage, neutral balance). Pure with respect to
 * globals: all state is carried in a caller-owned ::pi_controller_t, so the block is unit-testable.
 * Anti-windup matters during pre-charge/saturation: the integrator must not run away while the
 * output is clamped.
 **************************************************************************************************************************/
#ifndef PI_CONTROLLER_H_
#define PI_CONTROLLER_H_

/*! Static configuration for one PI instance. */
typedef struct
{
    float kp;          /*!< Proportional gain                                            */
    float ki;          /*!< Integral gain                                                */
    float out_min;     /*!< Output saturation lower limit                                */
    float out_max;     /*!< Output saturation upper limit                                */
    float kaw;         /*!< Back-calculation anti-windup gain                            */
    float dt;          /*!< Sample period [s]                                            */
} pi_params_t;

/*! Mutable run-time state for one PI instance. */
typedef struct
{
    float integrator;  /*!< Accumulated integral term                                    */
} pi_controller_t;

/*! Reset the integral state (e.g. on entering/leaving a control mode). */
void  pi_reset(pi_controller_t * p_pi);

/*!
 * Advance one PI step.
 * @param p_pi      controller state (updated in place)
 * @param p_params  controller configuration
 * @param error     setpoint - measurement
 * @return saturated controller output
 */
float pi_step(pi_controller_t * p_pi, const pi_params_t * p_params, float error);

#endif /* PI_CONTROLLER_H_ */
