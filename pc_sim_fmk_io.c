#include "pc_sim_runtime.h"

#include <string.h>

#if (FMKIO_INPUT_SIGFREQ_NB > 0U)
#define PCSIM_IN_FREQ_SLOT_NB FMKIO_INPUT_SIGFREQ_NB
#else
#define PCSIM_IN_FREQ_SLOT_NB 1U
#endif

#if (FMKIO_INPUT_ENCODER_NB > 0U)
#define PCSIM_ENCODER_SLOT_NB FMKIO_INPUT_ENCODER_NB
#else
#define PCSIM_ENCODER_SLOT_NB 1U
#endif

const t_float32 *PCSIM_GetAnalogSnapshot(void)
{
    return g_pcSimAnalogValues_af32;
}

const t_uint16 *PCSIM_GetPwmSnapshot(void)
{
    return g_pcSimPwmDuty_au16;
}

t_eReturnCode PCSIM_SetAnalogInput(t_eFMKIO_InAnaSig signal, t_float32 value)
{
    if (signal >= FMKIO_INPUT_SIGANA_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    g_pcSimAnalogValues_af32[signal] = value;
    return RC_OK;
}

t_eReturnCode PCSIM_GetAnalogInput(t_eFMKIO_InAnaSig signal, t_float32 *value)
{
    if (signal >= FMKIO_INPUT_SIGANA_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (value == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *value = g_pcSimAnalogValues_af32[signal];
    return RC_OK;
}

t_eReturnCode PCSIM_SetInputDigital(t_eFMKIO_InDigSig signal, t_eFMKIO_DigValue value)
{
    if (signal >= FMKIO_INPUT_SIGDIG_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    g_pcSimInDig_ae[signal] = value;
    return RC_OK;
}

t_eReturnCode PCSIM_GetInputDigital(t_eFMKIO_InDigSig signal, t_eFMKIO_DigValue *value)
{
    if (signal >= FMKIO_INPUT_SIGDIG_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (value == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *value = g_pcSimInDig_ae[signal];
    return RC_OK;
}

t_eReturnCode PCSIM_SetInputFrequency(t_eFMKIO_InFreqSig signal, t_float32 value)
{
    if (signal >= PCSIM_IN_FREQ_SLOT_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    g_pcSimInFreq_af32[signal] = value;
    return RC_OK;
}

t_eReturnCode PCSIM_GetInputFrequency(t_eFMKIO_InFreqSig signal, t_float32 *value)
{
    if (signal >= PCSIM_IN_FREQ_SLOT_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (value == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *value = g_pcSimInFreq_af32[signal];
    return RC_OK;
}

t_eReturnCode PCSIM_SetEncoderPosition(t_eFMKIO_InEcdrSignals signal, t_float32 absolute, t_float32 relative)
{
    if (signal >= PCSIM_ENCODER_SLOT_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    g_pcSimEncAbs_af32[signal] = absolute;
    g_pcSimEncRel_af32[signal] = relative;
    return RC_OK;
}

t_eReturnCode PCSIM_GetEncoderPosition(t_eFMKIO_InEcdrSignals signal, t_float32 *absolute, t_float32 *relative)
{
    if (signal >= PCSIM_ENCODER_SLOT_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if ((absolute == NULL) || (relative == NULL))
    {
        return RC_ERROR_PTR_NULL;
    }
    *absolute = g_pcSimEncAbs_af32[signal];
    *relative = g_pcSimEncRel_af32[signal];
    return RC_OK;
}

t_eReturnCode PCSIM_SetEncoderSpeed(t_eFMKIO_InEcdrSignals signal, t_float32 speed)
{
    if (signal >= PCSIM_ENCODER_SLOT_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    g_pcSimEncSpeed_af32[signal] = speed;
    return RC_OK;
}

t_eReturnCode PCSIM_GetEncoderSpeed(t_eFMKIO_InEcdrSignals signal, t_float32 *speed)
{
    if (signal >= PCSIM_ENCODER_SLOT_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (speed == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *speed = g_pcSimEncSpeed_af32[signal];
    return RC_OK;
}

t_eReturnCode PCSIM_SetPwmDuty(t_eFMKIO_OutPwmSig signal, t_uint16 duty)
{
    if (signal >= FMKIO_OUTPUT_SIGPWM_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    g_pcSimPwmDuty_au16[signal] = duty;
    return RC_OK;
}

t_eReturnCode PCSIM_SetPwmFrequency(t_eFMKIO_OutPwmSig signal, t_float32 frequency)
{
    if (signal >= FMKIO_OUTPUT_SIGPWM_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    g_pcSimPwmFreq_af32[signal] = frequency;
    return RC_OK;
}

t_eReturnCode PCSIM_GetPwmFrequency(t_eFMKIO_OutPwmSig signal, t_float32 *frequency)
{
    if (signal >= FMKIO_OUTPUT_SIGPWM_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (frequency == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *frequency = g_pcSimPwmFreq_af32[signal];
    return RC_OK;
}

t_eReturnCode PCSIM_SetPwmPulses(t_eFMKIO_OutPwmSig signal, t_uint16 pulses)
{
    if (signal >= FMKIO_OUTPUT_SIGPWM_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    g_pcSimPwmPulses_au16[signal] = pulses;
    return RC_OK;
}

t_eReturnCode PCSIM_GetPwmPulses(t_eFMKIO_OutPwmSig signal, t_uint16 *pulses)
{
    if (signal >= FMKIO_OUTPUT_SIGPWM_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (pulses == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *pulses = g_pcSimPwmPulses_au16[signal];
    return RC_OK;
}

t_eReturnCode PCSIM_GetPwmDuty(t_eFMKIO_OutPwmSig signal, t_uint16 *duty)
{
    if (signal >= FMKIO_OUTPUT_SIGPWM_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (duty == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *duty = g_pcSimPwmDuty_au16[signal];
    return RC_OK;
}

t_eReturnCode FMKIO_Init(void) { return RC_OK; }
t_eReturnCode FMKIO_Cyclic(void) { return RC_OK; }

t_eReturnCode FMKIO_GetState(t_eCyclicModState *f_State_pe)
{
    if (f_State_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_State_pe = g_pcSimIoState_e;
    return RC_OK;
}

t_eReturnCode FMKIO_SetState(t_eCyclicModState f_State_e)
{
    g_pcSimIoState_e = f_State_e;
    return RC_OK;
}

t_eReturnCode FMKIO_Set_InDigSigCfg(t_eFMKIO_InDigSig f_signal_e, t_eFMKIO_PullMode f_pull_e)
{
    (void)f_signal_e;
    (void)f_pull_e;
    return RC_OK;
}

t_eReturnCode FMKIO_Set_InAnaSigCfg(t_eFMKIO_InAnaSig f_signal_e,
                                    t_sFMKIO_InAnaTresHoldCfg *f_tresHoldCfg_ps,
                                    t_bool f_enableTresholdMntor_b,
                                    t_cbFMKIO_SigErrorMngmt *f_sigErr_cb)
{
    (void)f_signal_e;
    (void)f_tresHoldCfg_ps;
    (void)f_enableTresholdMntor_b;
    (void)f_sigErr_cb;
    return RC_OK;
}

t_eReturnCode FMKIO_Set_InFreqSigCfg(t_eFMKIO_InFreqSig f_signal_e,
                                     t_eFMKIO_SigTrigCptr f_trigger_e,
                                     t_eFMKIO_FreqMeas f_freqMeas_e,
                                     t_float32 f_samplingHz_f32,
                                     t_cbFMKIO_SigErrorMngmt *f_sigErr_cb)
{
    (void)f_signal_e;
    (void)f_trigger_e;
    (void)f_freqMeas_e;
    (void)f_samplingHz_f32;
    (void)f_sigErr_cb;
    return RC_OK;
}

t_eReturnCode FMKIO_Set_InEvntSigCfg(t_eFMKIO_InEvntSig f_signal_e,
                                     t_eFMKIO_PullMode f_pull_e,
                                     t_eFMKIO_SigTrigCptr f_trigger_e,
                                     t_uint32 f_debouncDelay_u32,
                                     t_cbFMKIO_EventFunc *f_Evnt_cb,
                                     t_cbFMKIO_SigErrorMngmt *f_sigErr_cb)
{
    (void)f_signal_e;
    (void)f_pull_e;
    (void)f_trigger_e;
    (void)f_debouncDelay_u32;
    (void)f_Evnt_cb;
    (void)f_sigErr_cb;
    return RC_OK;
}

t_eReturnCode FMKIO_Set_InEncoderSigCfg(t_eFMKIO_InEcdrSignals f_InEncdr_e,
                                        t_sFMKIO_SigEcdrCfg f_encdrCfg_s,
                                        t_eFMKIO_EcdrStartOpe f_startOpe)
{
    (void)f_InEncdr_e;
    (void)f_encdrCfg_s;
    (void)f_startOpe;
    return RC_OK;
}

t_eReturnCode FMKIO_Set_InEcdrCalibOffset(t_eFMKIO_InEcdrSignals f_InEncdr_e, t_float32 f_calibValue_mrad_f32)
{
    if (f_InEncdr_e >= PCSIM_ENCODER_SLOT_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    g_pcSimEncRel_af32[f_InEncdr_e] = f_calibValue_mrad_f32;
    return RC_OK;
}

t_eReturnCode FMKIO_Set_OutPwmSigCfg(t_eFMKIO_OutPwmSig f_signal_e,
                                     t_sFMKIO_PwmWaveformCfg f_sigPwmCfg_s,
                                     t_sFMKIO_PwmControlPrm f_sigCtrlPrm_s,
                                     t_cbFMKIO_PulseEvent *f_pulseEvnt_pcb,
                                     t_cbFMKIO_SigErrorMngmt *f_sigErr_cb)
{
    (void)f_sigPwmCfg_s;
    (void)f_sigCtrlPrm_s;
    (void)f_pulseEvnt_pcb;
    (void)f_sigErr_cb;
    if (f_signal_e >= FMKIO_OUTPUT_SIGPWM_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    return RC_OK;
}

t_eReturnCode FMKIO_Set_OutDigSigCfg(t_eFMKIO_OutDigSig f_signal_e,
                                     t_eFMKIO_PullMode f_pull_e,
                                     t_eFMKIO_SpdMode f_spd_e)
{
    (void)f_pull_e;
    (void)f_spd_e;
    if (f_signal_e >= FMKIO_OUTPUT_SIGDIG_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    return RC_OK;
}

t_eReturnCode FMKIO_Set_ComCanCfg(t_eFMKIO_ComSigCan f_SigCan_e)
{
    (void)f_SigCan_e;
    return RC_OK;
}

t_eReturnCode FMKIO_Set_ComSerialCfg(t_eFMKIO_ComSigSerial f_SigSerial_e)
{
    (void)f_SigSerial_e;
    return RC_OK;
}

t_eReturnCode FMKIO_Set_OutDigSigValue(t_eFMKIO_OutDigSig f_signal_e, t_eFMKIO_DigValue f_value_e)
{
    if (f_signal_e >= FMKIO_OUTPUT_SIGDIG_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    g_pcSimOutDig_ae[f_signal_e] = f_value_e;
    return RC_OK;
}

t_eReturnCode FMKIO_Get_InDigSigValue(t_eFMKIO_InDigSig f_signal_e, t_eFMKIO_DigValue *f_value_pe)
{
    if (f_signal_e >= FMKIO_INPUT_SIGDIG_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (f_value_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_value_pe = g_pcSimInDig_ae[f_signal_e];
    return RC_OK;
}

t_eReturnCode FMKIO_Get_InEcdrPositionValue(t_eFMKIO_InEcdrSignals f_signal_e,
                                            t_eFMKIO_EcdrValFormat f_format_e,
                                            t_float32 *f_absolutePos_pf32,
                                            t_float32 *f_relativePos_pf32)
{
    (void)f_format_e;
    if (f_signal_e >= PCSIM_ENCODER_SLOT_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if ((f_absolutePos_pf32 == NULL) || (f_relativePos_pf32 == NULL))
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_absolutePos_pf32 = g_pcSimEncAbs_af32[f_signal_e];
    *f_relativePos_pf32 = g_pcSimEncRel_af32[f_signal_e];
    return RC_OK;
}

t_eReturnCode FMKIO_Get_InEcdrSpeed(t_eFMKIO_InEcdrSignals f_InEncdr_e,
                                    t_eFMKIO_EcdrValFormat f_speedFormat_e,
                                    t_float32 *f_ecdrSpeed_pf32)
{
    (void)f_speedFormat_e;
    if (f_InEncdr_e >= PCSIM_ENCODER_SLOT_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (f_ecdrSpeed_pf32 == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_ecdrSpeed_pf32 = g_pcSimEncSpeed_af32[f_InEncdr_e];
    return RC_OK;
}

t_eReturnCode FMKIO_Get_InEcdrDirectionValue(t_eFMKIO_InEcdrSignals f_signal_e,
                                             t_eFMKIO_EcdrDir *f_Dirvalue_pe)
{
    if (f_signal_e >= PCSIM_ENCODER_SLOT_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (f_Dirvalue_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_Dirvalue_pe = g_pcSimEncDir_ae[f_signal_e];
    return RC_OK;
}

t_eReturnCode FMKIO_Get_InAnaSigValue(t_eFMKIO_InAnaSig f_signal_e, t_float32 *f_value_pf32)
{
    if (f_signal_e >= FMKIO_INPUT_SIGANA_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (f_value_pf32 == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_value_pf32 = g_pcSimAnalogValues_af32[f_signal_e];
    return RC_OK;
}

t_eReturnCode FMKIO_Get_InFreqSigValue(t_eFMKIO_InFreqSig f_signal_e, t_float32 *f_value_pf32)
{
    if (f_signal_e >= PCSIM_IN_FREQ_SLOT_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (f_value_pf32 == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_value_pf32 = g_pcSimInFreq_af32[f_signal_e];
    return RC_OK;
}

t_eReturnCode FMKIO_Set_OutPwmSigDutyCycle(t_eFMKIO_OutPwmSig f_signal_e, t_uint16 f_dutyCycle_u16)
{
    return PCSIM_SetPwmDuty(f_signal_e, f_dutyCycle_u16);
}

t_eReturnCode FMKIO_Set_OutPwmSigFrequency(t_eFMKIO_OutPwmSig f_signal_e, t_float32 f_frequency_f32)
{
    return PCSIM_SetPwmFrequency(f_signal_e, f_frequency_f32);
}

t_eReturnCode FMKIO_Set_OutPwmSigPulses(t_eFMKIO_OutPwmSig f_signal_e,
                                        t_float32 f_frequency_f32,
                                        t_uint16 f_dutyCycle_u16,
                                        t_uint16 f_pulses_u16)
{
    t_eReturnCode ret_e;
    ret_e = PCSIM_SetPwmFrequency(f_signal_e, f_frequency_f32);
    if (ret_e == RC_OK)
    {
        ret_e = PCSIM_SetPwmDuty(f_signal_e, f_dutyCycle_u16);
    }
    if (ret_e == RC_OK)
    {
        ret_e = PCSIM_SetPwmPulses(f_signal_e, f_pulses_u16);
    }
    return ret_e;
}

t_eReturnCode FMKIO_Get_OutPwmSigDutyCycle(t_eFMKIO_OutPwmSig f_signal_e, t_uint16 *f_dutyCycle_pu16)
{
    return PCSIM_GetPwmDuty(f_signal_e, f_dutyCycle_pu16);
}

t_eReturnCode FMKIO_Get_OutPwmSigFrequency(t_eFMKIO_OutPwmSig f_signal_e, t_float32 *f_frequency_pf32)
{
    return PCSIM_GetPwmFrequency(f_signal_e, f_frequency_pf32);
}

t_eReturnCode FMKIO_Get_OutDigSigValue(t_eFMKIO_OutDigSig f_signal_e, t_eFMKIO_DigValue *f_value_pe)
{
    if (f_signal_e >= FMKIO_OUTPUT_SIGDIG_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (f_value_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_value_pe = g_pcSimOutDig_ae[f_signal_e];
    return RC_OK;
}

void FMKIO_BspRqst_InterruptMngmt(void) {}
