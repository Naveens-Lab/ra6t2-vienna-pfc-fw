/***********************************************************************************************************************//**
 * @file    adc_manager.h
 * @brief   ADC encapsulation — the single clean sensor boundary
 *
 * The control loop must NEVER touch raw ADC registers, channel indices, or scaling math. This
 * module is the only thing that knows the ADC exists. It opens and HW-calibrates ADC_B, measures
 * the bipolar current offsets at boot, and exposes calibrated engineering-unit measurements through
 * a single atomic, double-buffered snapshot.
 *
 * Channel ordering (VC0..VC7 = Va,Vb,Vc,Ia,Ib,Ic,V+,V-) is internal to this module.
 **************************************************************************************************************************/
#ifndef ADC_MANAGER_H_
#define ADC_MANAGER_H_

#include "pfc_types.h"

/*! Open ADC_B and run hardware calibration. Halts safely on FSP error. */
void adc_manager_init(void);

/*! Average N samples at zero current to capture the per-current-channel zero offset (REQUIRED). */
void adc_manager_measure_offsets(void);

/*! Atomic, consistent snapshot of the latest converted measurements for the control loop. */
const pfc_measurements_t * adc_manager_latest(void);

/*! Called from adc_b_callback: reads the 8 channels, converts, fills the snapshot, signals the loop. */
void adc_manager_on_scan_complete(void);

#endif /* ADC_MANAGER_H_ */
