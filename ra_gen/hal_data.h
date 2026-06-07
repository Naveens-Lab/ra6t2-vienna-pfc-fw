/* generated HAL header file - do not edit */
#ifndef HAL_DATA_H_
#define HAL_DATA_H_
#include <stdint.h>
#include "bsp_api.h"
#include "common_data.h"
#include "r_acmphs.h"
#include "r_comparator_api.h"
#include "r_dac.h"
#include "r_dac_api.h"
#include "r_adc_b.h"
#include "r_adc_api.h"
#include "r_poeg.h"
#include "r_poeg_api.h"
#include "r_gpt.h"
#include "r_timer_api.h"
#include "r_gpt_three_phase.h"
#include "r_three_phase_api.h"
FSP_HEADER
/** Comparator Instance. */
extern const comparator_instance_t g_acmphs2;

/** Access the Comparator instance using these structures when calling API functions directly (::p_api is not used). */
extern acmphs_instance_ctrl_t g_acmphs2_ctrl;
extern const comparator_cfg_t g_acmphs2_cfg;

#ifndef NULL
void NULL(comparator_callback_args_t *p_args);
#endif
/** Comparator Instance. */
extern const comparator_instance_t g_acmphs1;

/** Access the Comparator instance using these structures when calling API functions directly (::p_api is not used). */
extern acmphs_instance_ctrl_t g_acmphs1_ctrl;
extern const comparator_cfg_t g_acmphs1_cfg;

#ifndef NULL
void NULL(comparator_callback_args_t *p_args);
#endif
/** Comparator Instance. */
extern const comparator_instance_t g_acmphs0;

/** Access the Comparator instance using these structures when calling API functions directly (::p_api is not used). */
extern acmphs_instance_ctrl_t g_acmphs0_ctrl;
extern const comparator_cfg_t g_acmphs0_cfg;

#ifndef NULL
void NULL(comparator_callback_args_t *p_args);
#endif
/** DAC on DAC Instance. */
extern const dac_instance_t g_dac0;

/** Access the DAC instance using these structures when calling API functions directly (::p_api is not used). */
extern dac_instance_ctrl_t g_dac0_ctrl;
extern const dac_cfg_t g_dac0_cfg;
/** ADC on ADC_B instance. */
extern const adc_instance_t g_adc_b;

/** Access the ADC_B instance using these structures when calling API functions directly (::p_api is not used). */
extern adc_b_instance_ctrl_t g_adc_b_ctrl;
extern const adc_cfg_t g_adc_b_cfg;
extern const adc_b_scan_cfg_t g_adc_b_scan_cfg;

#ifndef adc_b_callback
void adc_b_callback(adc_callback_args_t *p_args);
#endif
/** POEG Instance. */
extern const poeg_instance_t g_poeg_a;

/** Access the POEG instance using these structures when calling API functions directly (::p_api is not used). */
extern poeg_instance_ctrl_t g_poeg_a_ctrl;
extern const poeg_cfg_t g_poeg_a_cfg;

#ifndef poeg_a_callback
void poeg_a_callback(poeg_callback_args_t *p_args);
#endif
/** Timer on GPT Instance. */
extern const timer_instance_t g_timer2;

/** Access the GPT instance using these structures when calling API functions directly (::p_api is not used). */
extern gpt_instance_ctrl_t g_timer2_ctrl;
extern const timer_cfg_t g_timer2_cfg;

#ifndef NULL
void NULL(timer_callback_args_t *p_args);
#endif
/** Timer on GPT Instance. */
extern const timer_instance_t g_timer1;

/** Access the GPT instance using these structures when calling API functions directly (::p_api is not used). */
extern gpt_instance_ctrl_t g_timer1_ctrl;
extern const timer_cfg_t g_timer1_cfg;

#ifndef NULL
void NULL(timer_callback_args_t *p_args);
#endif
/** Timer on GPT Instance. */
extern const timer_instance_t g_timer0;

/** Access the GPT instance using these structures when calling API functions directly (::p_api is not used). */
extern gpt_instance_ctrl_t g_timer0_ctrl;
extern const timer_cfg_t g_timer0_cfg;

#ifndef NULL
void NULL(timer_callback_args_t *p_args);
#endif
/** GPT Three-Phase Instance. */
extern const three_phase_instance_t g_three_phase0;

/** Access the GPT Three-Phase instance using these structures when calling API functions directly (::p_api is not used). */
extern gpt_three_phase_instance_ctrl_t g_three_phase0_ctrl;
extern const three_phase_cfg_t g_three_phase0_cfg;
void hal_entry(void);
void g_hal_init(void);
FSP_FOOTER
#endif /* HAL_DATA_H_ */
