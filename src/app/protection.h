/***********************************************************************************************************************//**
 * @file    protection.h
 * @brief   Overcurrent protection chain: DAC (DA3 threshold) -> ACMPHS x3 -> POEG gate kill.
 *
 * Protection is hardware-fast and CPU-independent: the comparators trip POEG directly, which cuts
 * all six gate outputs in <100 ns without software. This module arms that chain and provides the
 * fault flag / handler. It must be live before any switching begins.
 **************************************************************************************************************************/
#ifndef PROTECTION_H_
#define PROTECTION_H_

#include <stdbool.h>

/*! Arm the OCP chain: write the DA3 threshold (DAC), open the 3 comparators and POEG. Safe-halt on error. */
void protection_init(void);

/*! @return true once a POEG fault has latched (gates already cut in hardware). */
bool protection_is_faulted(void);

/*! Software-commanded gate kill: force the POEG output-disable (cuts all six gates) and latch FAULT.
 *  For supervisor-detected faults (e.g. software over-voltage) that the hardware OCP does not cover. */
void protection_force_disable(void);

#endif /* PROTECTION_H_ */
