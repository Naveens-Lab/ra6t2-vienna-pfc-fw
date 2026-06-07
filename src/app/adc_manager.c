/***********************************************************************************************************************//**
 * @file    adc_manager.c
 * @brief   ADC manager implementation— the single clean sensor boundary.
 *
 * Owns ADC_B entirely: open + hardware calibrate, boot-time current-offset measurement, the
 * per-cycle read/convert into engineering units, and the atomic double-buffered snapshot the
 * control loop consumes. No other module knows the channel ordering, the scaling, or that the
 * ADC exists.
 *
 * Channel ordering (internal): VC0..VC7 map to ADC_CHANNEL_0..7 = Va,Vb,Vc,Ia,Ib,Ic,V+,V-.
 * For external channels, R_ADC_B_Read(ch_n) returns the converted result of ADC channel n
 * (register ADDR[n]), so the ADC-channel id is the read key.
 **************************************************************************************************************************/
#include "adc_manager.h"
#include "pfc_config.h"
#include "pfc_control.h"
#include "pfc_safe.h"
#include "hal_data.h"

/*--------------------------------------------------------------------------------------------------------------------*
 * Internal channel map (ADC channel id per measured signal)
 *--------------------------------------------------------------------------------------------------------------------*/
#define ADC_MGR_CH_VA     (ADC_CHANNEL_0)
#define ADC_MGR_CH_VB     (ADC_CHANNEL_1)
#define ADC_MGR_CH_VC     (ADC_CHANNEL_2)
#define ADC_MGR_CH_IA     (ADC_CHANNEL_3)
#define ADC_MGR_CH_IB     (ADC_CHANNEL_4)
#define ADC_MGR_CH_IC     (ADC_CHANNEL_5)
#define ADC_MGR_CH_VPOS   (ADC_CHANNEL_6)
#define ADC_MGR_CH_VNEG   (ADC_CHANNEL_7)

/*--------------------------------------------------------------------------------------------------------------------*
 * Module state
 *--------------------------------------------------------------------------------------------------------------------*/

static volatile pfc_measurements_t g_snapshot[2];
static volatile uint32_t           g_active_buffer = 0U;

/*! Bipolar current zero offsets [ADC counts], measured at boot by adc_manager_measure_offsets().
 *  Index order: Ia, Ib, Ic. Until measured, seeded with the configured mid-scale placeholder. */
static volatile float g_current_offset_counts[3] =
{
    PFC_OFFSET_ICURR_COUNTS, PFC_OFFSET_ICURR_COUNTS, PFC_OFFSET_ICURR_COUNTS
};

/*! Offset-calibration handshake between this (foreground) call and the scan-complete ISR. */
static volatile bool     g_offset_cal_active = false;
static volatile uint32_t g_offset_cal_count  = 0U;
static volatile uint32_t g_offset_sum[3]     = {0U, 0U, 0U};

/*--------------------------------------------------------------------------------------------------------------------*
 * Helpers
 *--------------------------------------------------------------------------------------------------------------------*/

/*! Read one external ADC channel's latest converted result [counts]. Error bit masked by FSP. */
static inline uint16_t adc_manager_read_raw (adc_channel_t channel)
{
    uint16_t raw = 0U;
    (void) R_ADC_B_Read(&g_adc_b_ctrl, channel, &raw);
    return raw;
}

/*--------------------------------------------------------------------------------------------------------------------*
 * Public API
 *--------------------------------------------------------------------------------------------------------------------*/

void adc_manager_init (void)
{
    fsp_err_t err;

    /* Open and configure the scan groups. */
    err = R_ADC_B_Open(&g_adc_b_ctrl, &g_adc_b_cfg);
    if (FSP_SUCCESS != err)
    {
        pfc_safe_halt();
    }

    err = R_ADC_B_ScanCfg(&g_adc_b_ctrl, &g_adc_b_scan_cfg);
    if (FSP_SUCCESS != err)
    {
        pfc_safe_halt();
    }

    /* Calibration is interrupt-driven and asynchronous; kick it off then wait (bounded) for the
     * converter to reach READY. ScanStart is rejected until calibration completes. */
    err = R_ADC_B_Calibrate(&g_adc_b_ctrl, NULL);
    if (FSP_SUCCESS != err)
    {
        pfc_safe_halt();
    }

    uint32_t guard = 0U;
    while (ADC_B_CONVERTER_STATE_READY != g_adc_b_ctrl.adc_state)
    {
        if (ADC_B_CONVERTER_STATE_CALIBRATION_FAIL == g_adc_b_ctrl.adc_state)
        {
            pfc_safe_halt();
        }
        if (++guard > PFC_INIT_WAIT_TIMEOUT)
        {
            pfc_safe_halt();
        }
    }

    /* Enable the GPT/hardware trigger path. Scans then fire on each carrier trough once the
     * 3-phase timer is running (started by the supervisor)*/
    err = R_ADC_B_ScanStart(&g_adc_b_ctrl);
    if (FSP_SUCCESS != err)
    {
        pfc_safe_halt();
    }
}

void adc_manager_measure_offsets (void)
{
    /* Capture the bipolar current zero — REQUIRED, or measured currents are wrong-signed.
     * Precondition: scans are running (GPT trough trigger active) and current is zero.
     * The scan-complete ISR accumulates raw current-channel counts while cal is active. */
    g_offset_sum[0]     = 0U;
    g_offset_sum[1]     = 0U;
    g_offset_sum[2]     = 0U;
    g_offset_cal_count  = 0U;
    g_offset_cal_active = true;

    uint32_t guard = 0U;
    while (g_offset_cal_count < PFC_OFFSET_CAL_SAMPLES)
    {
        if (++guard > PFC_INIT_WAIT_TIMEOUT)
        {
            /* Scans never arrived (GPT not running?) — leave the placeholder offsets and halt
             * rather than run with an unknown current zero. */
            g_offset_cal_active = false;
            pfc_safe_halt();
        }
    }
    g_offset_cal_active = false;

    const float n = (float) PFC_OFFSET_CAL_SAMPLES;
    g_current_offset_counts[0] = (float) g_offset_sum[0] / n;
    g_current_offset_counts[1] = (float) g_offset_sum[1] / n;
    g_current_offset_counts[2] = (float) g_offset_sum[2] / n;
}

const pfc_measurements_t * adc_manager_latest (void)
{
    /* Return the most recently published complete buffer. */
    return (const pfc_measurements_t *) &g_snapshot[g_active_buffer];
}

void adc_manager_on_scan_complete (void)
{
    /* During offset calibration, only accumulate the current channels and skip publishing. */
    if (g_offset_cal_active)
    {
        if (g_offset_cal_count < PFC_OFFSET_CAL_SAMPLES)
        {
            g_offset_sum[0] += adc_manager_read_raw(ADC_MGR_CH_IA);
            g_offset_sum[1] += adc_manager_read_raw(ADC_MGR_CH_IB);
            g_offset_sum[2] += adc_manager_read_raw(ADC_MGR_CH_IC);
            g_offset_cal_count++;
        }
        return;
    }

    /* Read all eight channels, then convert into engineering units. */
    const float va_raw = (float) adc_manager_read_raw(ADC_MGR_CH_VA);
    const float vb_raw = (float) adc_manager_read_raw(ADC_MGR_CH_VB);
    const float vc_raw = (float) adc_manager_read_raw(ADC_MGR_CH_VC);
    const float ia_raw = (float) adc_manager_read_raw(ADC_MGR_CH_IA);
    const float ib_raw = (float) adc_manager_read_raw(ADC_MGR_CH_IB);
    const float ic_raw = (float) adc_manager_read_raw(ADC_MGR_CH_IC);
    const float vp_raw = (float) adc_manager_read_raw(ADC_MGR_CH_VPOS);
    const float vn_raw = (float) adc_manager_read_raw(ADC_MGR_CH_VNEG);

    const uint32_t write_idx = g_active_buffer ^ 1U;
    volatile pfc_measurements_t * const measured_adc = &g_snapshot[write_idx];

    /* eng = (raw - offset) * scale. Voltages: fixed (ground-referenced) offset. Currents: measured. */
    measured_adc->va    = (va_raw - PFC_OFFSET_VGRID_COUNTS) * PFC_CAL_SCALE_VGRID;
    measured_adc->vb    = (vb_raw - PFC_OFFSET_VGRID_COUNTS) * PFC_CAL_SCALE_VGRID;
    measured_adc->vc    = (vc_raw - PFC_OFFSET_VGRID_COUNTS) * PFC_CAL_SCALE_VGRID;
    measured_adc->ia    = (ia_raw - g_current_offset_counts[0]) * PFC_CAL_SCALE_ICURR;
    measured_adc->ib    = (ib_raw - g_current_offset_counts[1]) * PFC_CAL_SCALE_ICURR;
    measured_adc->ic    = (ic_raw - g_current_offset_counts[2]) * PFC_CAL_SCALE_ICURR;
    measured_adc->v_pos = (vp_raw - PFC_OFFSET_VBUS_COUNTS) * PFC_CAL_SCALE_VBUS;
    measured_adc->v_neg = (vn_raw - PFC_OFFSET_VBUS_COUNTS) * PFC_CAL_SCALE_VBUS;

    /* Derived quantities computed once here so downstream code never re-derives them. */
    measured_adc->v_bus     = measured_adc->v_pos + measured_adc->v_neg;
    measured_adc->v_mid_err = measured_adc->v_pos - measured_adc->v_neg;

    /* Publish atomically: single-word store of the freshly written buffer index. */
    g_active_buffer = write_idx;

    /* Signal the control loop now that a fresh, consistent snapshot is available. Runs in this same
     * ISR context (the trough trigger) so the loop acts on the current cycle's samples. */
    pfc_control_isr();
}

/*******************************************************************************************************************//**
 * @brief FSP callback for ADC_B Scan-End Group 0 (control-loop clock)
 **********************************************************************************************************************/
void adc_b_callback (adc_callback_args_t * p_args)
{
    (void) p_args;
    adc_manager_on_scan_complete();
}
