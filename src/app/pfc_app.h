/***********************************************************************************************************************//**
 * @file    pfc_app.h
 * @brief   Top-level application: supervisor state machine and startup sequencing.
 *
 * Owns the safe power-up order (INIT -> PRECHARGE -> PLL_LOCK -> RAMP -> RUN -> FAULT): pre-charge
 * the bus, wait for PLL lock, ramp Id_ref gently, then enable full regulation. Energising without
 * pre-charge means inrush; running without PLL lock means an invalid theta and uncontrolled currents.
 *
 * Called from hal_entry(): pfc_app_init() once, then pfc_app_run() forever.
 **************************************************************************************************************************/
#ifndef PFC_APP_H_
#define PFC_APP_H_

#include "pfc_types.h"

/*! Initialise all subsystems (ADC, protection, control) in safe order. Halts safely on failure. */
void pfc_app_init(void);

/*! Background supervisor step: advances the state machine. Runs in the main loop (not the ISR). */
void pfc_app_run(void);

/*! @return current supervisor state (for debug / telemetry). */
pfc_state_t pfc_app_state(void);

#endif /* PFC_APP_H_ */
