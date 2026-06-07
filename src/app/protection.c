/***********************************************************************************************************************//**
 * @file    protection.c
 * @brief   Overcurrent protection DAC DA3 threshold -> ACMPHS x3 -> POEG hardware gate kill.
 *
 * The trip path is entirely hardware and CPU-independent: each phase current (via its sense amp on
 * AN012/13/14) feeds a comparator whose reference is the shared DA3 threshold; on a rising edge the
 * comparator output drives POEG group A, which disables all six GPT gate outputs in <100 ns. The MCU
 * is not in the loop. This module only ARMS that chain and LATCHES the fault for the supervisor.
 *
 * Arming order matters: bring up the DAC reference first (the comparators measure against it), then
 * open POEG (the sink), then open each comparator and enable its output (the source) after the
 * mandatory stabilisation wait so a settling transient cannot false-trip.
 *
 * FSP handles (verified against ra_gen/hal_data.h):
 *   DAC threshold  -> g_dac0           (channel 3 = DA3)
 *   comparators    -> g_acmphs0 (ch0=Ia), g_acmphs1 (ch1=Ib), g_acmphs2 (ch2=Ic)
 *   gate kill      -> g_poeg_a         (group A; callback poeg_a_callback)
 **************************************************************************************************************************/
#include "protection.h"
#include "pfc_config.h"
#include "pfc_safe.h"
#include "hal_data.h"

/*! Latched on the first POEG event; cleared only by a full re-init. Shared with the ISR -> volatile. */
static volatile bool g_faulted = false;

/*! The three OCP comparators, indexed by phase (A,B,C = channels 0,1,2). */
static const comparator_instance_t * const gp_comparators[3] =
{
    &g_acmphs0, &g_acmphs1, &g_acmphs2
};

void protection_init (void)
{
    fsp_err_t err;

    g_faulted = false;

    /* 1. DAC -> DA3 = shared OCP threshold. Write before start, then let it settle. */
    err = R_DAC_Open(&g_dac0_ctrl, &g_dac0_cfg);
    if (FSP_SUCCESS != err)
    {
        pfc_safe_halt();
    }
    err = R_DAC_Write(&g_dac0_ctrl, (uint16_t) PFC_OCP_THRESHOLD_COUNTS);
    if (FSP_SUCCESS != err)
    {
        pfc_safe_halt();
    }
    err = R_DAC_Start(&g_dac0_ctrl);
    if (FSP_SUCCESS != err)
    {
        pfc_safe_halt();
    }
    R_BSP_SoftwareDelay(PFC_DAC_SETTLE_US, BSP_DELAY_UNITS_MICROSECONDS);

    /* 2. POEG group A: the gate-kill sink the comparators will drive. */
    err = R_POEG_Open(&g_poeg_a_ctrl, &g_poeg_a_cfg);
    if (FSP_SUCCESS != err)
    {
        pfc_safe_halt();
    }

    /* 3. Comparators: open each, wait its stabilisation time, then enable output to POEG. */
    for (uint32_t i = 0U; i < 3U; i++)
    {
        const comparator_instance_t * p_cmp = gp_comparators[i];

        err = p_cmp->p_api->open(p_cmp->p_ctrl, p_cmp->p_cfg);
        if (FSP_SUCCESS != err)
        {
            pfc_safe_halt();
        }

        comparator_info_t info;
        err = p_cmp->p_api->infoGet(p_cmp->p_ctrl, &info);
        if (FSP_SUCCESS != err)
        {
            pfc_safe_halt();
        }
        R_BSP_SoftwareDelay(info.min_stabilization_wait_us, BSP_DELAY_UNITS_MICROSECONDS);

        err = p_cmp->p_api->outputEnable(p_cmp->p_ctrl);
        if (FSP_SUCCESS != err)
        {
            pfc_safe_halt();
        }
    }
}

bool protection_is_faulted (void)
{
    return g_faulted;
}

void protection_force_disable (void)
{
    /* Force the POEG software output-disable: cuts all six gates, then latch. */
    (void) R_POEG_OutputDisable(&g_poeg_a_ctrl);
    g_faulted = true;
}

/*******************************************************************************************************************//**
 * @brief FSP callback for the POEG fault event. Gates are already cut in hardware; we only latch.
 *        NOTE: name fixed by generated code (ra_gen/hal_data.h) — do not rename.
 **********************************************************************************************************************/
void poeg_a_callback (poeg_callback_args_t * p_args)
{
    (void) p_args;
    g_faulted = true;
}
