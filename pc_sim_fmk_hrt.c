#include "pc_sim_runtime.h"
#include "FMK_HAL/FMK_HRT/Src/FMK_HRT.h"

t_eReturnCode FMKHRT_Init(void) { return RC_OK; }
t_eReturnCode FMKHRT_Cyclic(void) { return RC_OK; }

t_eReturnCode FMKHRT_GetState(t_eCyclicModState *f_State_pe)
{
    if (f_State_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_State_pe = g_pcSimHrtState_e;
    return RC_OK;
}

t_eReturnCode FMKHRT_SetState(t_eCyclicModState f_State_e)
{
    g_pcSimHrtState_e = f_State_e;
    return RC_OK;
}

t_eReturnCode FMKHRT_ConfigurePwmLine(t_eFMKHRT_HighResLine f_HRLine_e,
                                      t_eFMKHRT_FreqMulDiv f_CpuFreqMulDiv_e,
                                      t_sFMKHRT_PwmCfg f_PwmCfg_s,
                                      t_cbFMKHRT_HrLineEvnt *f_pulseEvntCb_pcb)
{
    (void)f_HRLine_e;
    (void)f_CpuFreqMulDiv_e;
    (void)f_PwmCfg_s;
    (void)f_pulseEvntCb_pcb;
    return RC_OK;
}

t_eReturnCode FMKHRT_SetPwmLineWaveform(t_eFMKHRT_HighResLine f_HRLine_e,
                                        t_sFMKHRT_PwmOpeVal f_PwmOpe_s,
                                        t_uint8 f_maskUpdate_u8)
{
    (void)f_HRLine_e;
    (void)f_PwmOpe_s;
    (void)f_maskUpdate_u8;
    return RC_OK;
}

t_eReturnCode FMKHRT_GetPwmLineWaveform(t_eFMKHRT_HighResLine f_HRLine_e,
                                        t_sFMKHRT_PwmOpeVal *f_PwmVal_ps,
                                        t_uint8 f_maskUpdate_u8)
{
    (void)f_HRLine_e;
    (void)f_maskUpdate_u8;
    if (f_PwmVal_ps == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    f_PwmVal_ps->frequency_f32 = 0.0f;
    f_PwmVal_ps->dutyCycle_u16 = 0U;
    f_PwmVal_ps->nbPulses_u16 = 0U;
    return RC_OK;
}

HRTIM_HandleTypeDef *FMKHRT_PRIVATE_GetHandleTypeDef(t_eFMKHRT_HighResLine f_HRLine_e)
{
    (void)f_HRLine_e;
    return (HRTIM_HandleTypeDef *)NULL;
}
