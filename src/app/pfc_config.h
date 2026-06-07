/***********************************************************************************************************************//**
 * @file    pfc_config.h
 * @brief   Central configuration: every tunable constant for the Vienna PFC firmware.
 *
 * This is the ONLY place application constants live 
 * Sensor scaling, control gains, timing dividers, and protection thresholds are all named
 * and justified here so the control modules stay free of literals.
 *
 * @note  Many values are placeholders for Stage-1 scaffolding  Each is tagged
 *@c TODO and must be replaced with the measured hardware value before bring-up.
 **************************************************************************************************************************/
#ifndef PFC_CONFIG_H_
#define PFC_CONFIG_H_

/*--------------------------------------------------------------------------------------------------------------------*
 * Switching / control timing
 *  - PWM is center-aligned ~65 kHz (FSP: g_timer0 period_counts -> 15.383 us actual).
 *  - The control loop runs once per PWM cycle, clocked by the ADC trough trigger.
 *--------------------------------------------------------------------------------------------------------------------*/
#define PFC_PWM_FREQ_HZ            (65000.0f)                  /*!< Nominal carrier / control-loop rate [Hz]          */
#define PFC_CTRL_PERIOD_S          (1.0f / PFC_PWM_FREQ_HZ)    /*!< Control-loop sample period dt [s]                 */

/*! Outer (voltage) and neutral-balance loops are slow; run them every Nth control cycle to
 *  save ISR time and avoid destabilising fast dynamics with a too-fast outer loop. */
#define PFC_SLOW_LOOP_DIVIDER      (16U)

/*--------------------------------------------------------------------------------------------------------------------*
 * ADC front end (Vref = 3.3 V, 12-bit unsigned, channels AN000..AN007 all are configured in FSU)
 *  Calibration model per channel:  eng = (raw - offset) * scale
 *--------------------------------------------------------------------------------------------------------------------*/
#define PFC_ADC_VREF_VOLTS         (3.3f)                                       /*!< ADC reference voltage [V]        */
#define PFC_ADC_RESOLUTION_BITS    (12U)                                        /*!< ADC resolution                  */
#define PFC_ADC_FULL_SCALE_COUNTS  (4096.0f)                                    /*!< 2^12 counts                     */
#define PFC_ADC_COUNTS_TO_VOLTS    (PFC_ADC_VREF_VOLTS / PFC_ADC_FULL_SCALE_COUNTS) /*!< counts -> ADC-pin volts     */


/*! @name Per-channel scale factors (ADC-pin volts -> engineering units)
 *  TODO: replace placeholders (pass-through 1.0f) with the actual divider / sense-amp gains.
 *        Voltage channels: 1 / divider_ratio.   Current channels: 1 / (shunt * amp_gain).
 *  @{ */
#define PFC_SCALE_VGRID            (1.0f)   /*!< TODO: ADC volts -> grid phase voltage [V/V]  (Va,Vb,Vc) gain            */
#define PFC_SCALE_VBUS             (1.0f)   /*!< TODO: ADC volts -> bus rail voltage   [V/V]  (V+,V-) gain               */
#define PFC_SCALE_ICURR            (1.0f)   /*!< TODO: ADC volts -> inductor current   [A/V]  (Ia,Ib,Ic)  gain          */
/*! @} */

/*! @name Per-channel zero offsets [ADC counts]
 *  Voltages are single-ended to ground -> 0 offset. Currents are bipolar and biased to mid-rail;
 *  PFC_OFFSET_ICURR_COUNTS is only a startup placeholder and is overwritten by
 *  adc_manager_measure_offsets() (measured at zero current — REQUIRED for correct current sign).
 *  @{ */
#define PFC_OFFSET_VGRID_COUNTS    (0.0f)
#define PFC_OFFSET_VBUS_COUNTS     (0.0f)
#define PFC_OFFSET_ICURR_COUNTS    (2048.0f)   /*!< placeholder mid-scale; replaced at boot                           */
/*! @} */

/*! @name Derived per-count calibration scales (counts -> engineering units)
 *  Combines the ADC counts->volts step with the channel's volts->engineering factor, so the
 *  manager's hot path is a single multiply per channel:  eng = (raw - offset_counts) * scale.
 *  @{ */
#define PFC_CAL_SCALE_VGRID        (PFC_ADC_COUNTS_TO_VOLTS * PFC_SCALE_VGRID)  /*!< counts -> grid V   */
#define PFC_CAL_SCALE_VBUS         (PFC_ADC_COUNTS_TO_VOLTS * PFC_SCALE_VBUS)   /*!< counts -> rail V   */
#define PFC_CAL_SCALE_ICURR        (PFC_ADC_COUNTS_TO_VOLTS * PFC_SCALE_ICURR)  /*!< counts -> current A*/
/*! @} */

/*! Number of samples averaged per channel during current-offset calibration at boot. */
#define PFC_OFFSET_CAL_SAMPLES     (256U)

/*! Bounded spin limit for init waits (calibration / offset scans). Prevents a hang if the
 *  expected hardware event never arrives — on timeout the caller takes a safe halt. */
#define PFC_INIT_WAIT_TIMEOUT      (10000000U)

/*--------------------------------------------------------------------------------------------------------------------*
 * SRF-PLL (grid synchronisation)
 *  The phase detector is NORMALISED (err = Vq / |V| ~= sin(theta_err)), so the loop gain is
 *  independent of grid amplitude and the scaling placeholders above. Gains follow the standard
 *  2nd-order design for a normalised detector (Kpd = 1):
 *      wn = 2*pi*bandwidth ;  Kp = 2*zeta*wn ;  Ki = wn^2.
 *--------------------------------------------------------------------------------------------------------------------*/
#define PFC_PI                      (3.14159265358979f)
#define PFC_TWO_PI                  (2.0f * PFC_PI)

#define PFC_GRID_FREQ_NOMINAL_HZ   (60.0f)  /*!< Starting frequency estimate (480 V 3φ is typically 60 Hz NA);
                                                 the PLL pulls to the true grid frequency regardless.            */
#define PFC_GRID_FREQ_MIN_HZ       (45.0f)  /*!< Lower clamp on the frequency estimate (runaway guard)           */
#define PFC_GRID_FREQ_MAX_HZ       (65.0f)  /*!< Upper clamp on the frequency estimate                           */

#define PFC_GRID_OMEGA_NOMINAL     (PFC_TWO_PI * PFC_GRID_FREQ_NOMINAL_HZ) /*!< Nominal omega [rad/s]             */
#define PFC_PLL_OMEGA_MIN          (PFC_TWO_PI * PFC_GRID_FREQ_MIN_HZ)
#define PFC_PLL_OMEGA_MAX          (PFC_TWO_PI * PFC_GRID_FREQ_MAX_HZ)
#define PFC_PLL_DOMEGA_MIN         (PFC_PLL_OMEGA_MIN - PFC_GRID_OMEGA_NOMINAL) /*!< integrator (Δω) clamp lo      */
#define PFC_PLL_DOMEGA_MAX         (PFC_PLL_OMEGA_MAX - PFC_GRID_OMEGA_NOMINAL) /*!< integrator (Δω) clamp hi      */

#define PFC_PLL_ZETA               (0.707f) /*!< Loop damping ratio (critically-ish damped)                      */
#define PFC_PLL_BANDWIDTH_HZ       (15.0f)  /*!< Loop natural frequency [Hz] — fast lock vs. noise (TODO: tune)  */
#define PFC_PLL_WN                 (PFC_TWO_PI * PFC_PLL_BANDWIDTH_HZ)
#define PFC_PLL_KP                 (2.0f * PFC_PLL_ZETA * PFC_PLL_WN)  /*!< normalised-detector proportional gain  */
#define PFC_PLL_KI                 (PFC_PLL_WN * PFC_PLL_WN)           /*!< normalised-detector integral gain      */

#define PFC_PLL_MAG_EPS            (1.0e-6f) /*!< guard: skip normalisation when |V| ~ 0 (no signal)             */
#define PFC_PLL_LOCK_ERR_THRESH    (0.035f)  /*!< |normalised err| band for lock ~ sin(2 deg)                    */
#define PFC_PLL_LOCK_DWELL_CYCLES  (1000U)   /*!< consecutive in-band cycles to assert lock (~15 ms @ 65 kHz)    */

/*--------------------------------------------------------------------------------------------------------------------*
 * Control-loop PI gains (TODO: tune; placeholders are zero -> no actuation in the stub)
 *--------------------------------------------------------------------------------------------------------------------*/
#define PFC_PI_ID_KP               (0.0f)   /*!< Inner current loop, d-axis                                          */
#define PFC_PI_ID_KI               (0.0f)
#define PFC_PI_IQ_KP               (0.0f)   /*!< Inner current loop, q-axis (ref = 0 for unity PF)                   */
#define PFC_PI_IQ_KI               (0.0f)
#define PFC_PI_VBUS_KP             (0.0f)   /*!< Outer voltage loop (Vbus -> Id_ref)                                 */
#define PFC_PI_VBUS_KI             (0.0f)
#define PFC_PI_BAL_KP              (0.0f)   /*!< Neutral-balance loop ((V+)-(V-) -> offset)                          */
#define PFC_PI_BAL_KI              (0.0f)

/*! Back-calculation anti-windup gain shared by the PI blocks (TODO: tune, typ. ~1/Kp). */
#define PFC_PI_ANTIWINDUP_GAIN     (0.0f)

/*--------------------------------------------------------------------------------------------------------------------*
 * Setpoints and limits
 *--------------------------------------------------------------------------------------------------------------------*/
#define PFC_VBUS_REF_VOLTS         (800.0f) /*!< Total split-bus voltage target [V]                                  */
#define PFC_IQ_REF                 (0.0f)   /*!< Reactive-current reference = 0 (unity power factor)                  */

#define PFC_DUTY_MIN               (0.0f)   /*!< SVPWM duty clamp lower bound                                        */
#define PFC_DUTY_MAX               (1.0f)   /*!< SVPWM duty clamp upper bound                                        */

#define PFC_ID_REF_MAX_A           (0.0f)   /*!< TODO: max commanded active current [A] (voltage-loop output clamp)  */
#define PFC_ID_RAMP_A_PER_CYCLE    (0.0f)   /*!< TODO: Id_ref soft-start ramp rate [A per control cycle]             */

/*! @name Inner current-loop output clamps (d/q voltage commands [V])
 *  Bound the linear modulation region. TODO: ideally derived at runtime from v_bus (~ v_bus/sqrt(3));
 *  these compile-time placeholders are a safe static bound until that is wired in Task 7.
 *  @{ */
#define PFC_VD_MAX                 (500.0f)
#define PFC_VQ_MAX                 (500.0f)
/*! @} */

/*! Neutral-balance loop output clamp: max duty offset applied to all phases [normalised 0..1]. */
#define PFC_BALANCE_OFFSET_MAX     (0.10f)  /*!< TODO: tune; 10% duty headroom for midpoint correction */

/*! Slow-loop sample period [s] (outer voltage + neutral balance run every Nth control cycle). */
#define PFC_SLOW_LOOP_PERIOD_S     (PFC_CTRL_PERIOD_S * (float) PFC_SLOW_LOOP_DIVIDER)

/*--------------------------------------------------------------------------------------------------------------------*
 * Startup sequencing / supervisor (Task 8)
 *  Thresholds in engineering units depend on the (still-placeholder) voltage scaling; the timeouts
 *  are absolute. PFC_SECONDS_TO_CYCLES converts a wall-clock timeout to control cycles, using the
 *  free-running ISR tick counter as the supervisor's time base.
 *--------------------------------------------------------------------------------------------------------------------*/
#define PFC_PRECHARGE_DONE_V       (0.90f * PFC_VBUS_REF_VOLTS)  /*!< bus considered pre-charged at/above this [V]   */
#define PFC_PRECHARGE_TIMEOUT_S    (3.0f)                        /*!< max time to reach pre-charge before FAULT [s]  */
#define PFC_PLL_LOCK_TIMEOUT_S     (1.0f)                        /*!< max time to acquire PLL lock before FAULT [s]  */
#define PFC_VBUS_OV_V              (1.10f * PFC_VBUS_REF_VOLTS)  /*!< software over-voltage trip [V]                 */

#define PFC_SECONDS_TO_CYCLES(s)   ((uint32_t) ((s) * PFC_PWM_FREQ_HZ))

/*--------------------------------------------------------------------------------------------------------------------*
 * Protection (overcurrent: DAC DA3 -> ACMPHS IVREF -> POEG gate kill)
 *--------------------------------------------------------------------------------------------------------------------*/
#define PFC_DAC_FULL_SCALE_COUNTS  (4096U)  /*!< 12-bit DAC                                                          */
#define PFC_OCP_THRESHOLD_COUNTS   (2048U)  /*!< TODO: DA3 code = OCP trip level; placeholder mid-scale              */
#define PFC_DAC_SETTLE_US          (10U)    /*!< Settle time after DAC start before comparators use DA3 as IVREF [us] */

#endif /* PFC_CONFIG_H_ */
