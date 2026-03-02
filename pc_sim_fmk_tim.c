#include "pc_sim_runtime.h"

t_eReturnCode FMKTIM_Init(void) { return RC_OK; }

t_eReturnCode FMKTIM_Cyclic(void)
{
    t_uint32 now_u32;

    if ((g_pcSimFastTimer_s.configured_b == TRUE) &&
        (g_pcSimFastTimer_s.running_b == TRUE) &&
        (g_pcSimFastTimer_s.callback_pcb != NULL_FUNCTION))
    {
        now_u32 = PCSIM_GetTickMs();
        while (((t_float32)(now_u32 - g_pcSimFastTimer_s.lastTickMs_u32)) >= g_pcSimFastTimer_s.periodMs_f32)
        {
            g_pcSimFastTimer_s.lastTickMs_u32 += (t_uint32)g_pcSimFastTimer_s.periodMs_f32;
            g_pcSimFastTimer_s.callback_pcb(FMKTIM_INTERRUPT_LINE_TYPE_EVNT,
                                            (t_uint8)FMKTIM_INTERRUPT_LINE_EVNT_1);
            now_u32 = PCSIM_GetTickMs();
        }
    }

    return RC_OK;
}

t_eReturnCode FMKTIM_GetState(t_eCyclicModState *f_State_pe)
{
    if (f_State_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_State_pe = g_pcSimTimState_e;
    return RC_OK;
}

t_eReturnCode FMKTIM_SetState(t_eCyclicModState f_State_e)
{
    g_pcSimTimState_e = f_State_e;
    return RC_OK;
}

t_eReturnCode FMKTIM_Set_PWMLineCfg(t_eFMKTIM_InterruptLineIO f_InterruptLine_e,
                                    t_float32 f_pwmFreq_f32,
                                    t_eFMKTIM_LinePolarity f_linePolarity_e,
                                    t_bool f_enableSyncPulse_b,
                                    t_cbFMKTIM_InterruptLine *f_PwmPulseFinished_pcb)
{
    (void)f_InterruptLine_e;
    (void)f_pwmFreq_f32;
    (void)f_linePolarity_e;
    (void)f_enableSyncPulse_b;
    (void)f_PwmPulseFinished_pcb;
    return RC_OK;
}

t_eReturnCode FMKTIM_Set_EcdrLineCfg(t_eFMKTIM_InterruptLineIO f_InterruptLine_e,
                                     t_sFMKTIM_EcdrCfg f_EcdrCfg_s,
                                     t_uint32 f_ARRValue_u32)
{
    (void)f_InterruptLine_e;
    (void)f_EcdrCfg_s;
    (void)f_ARRValue_u32;
    return RC_OK;
}

t_eReturnCode FMKTIM_Set_ICLineCfg(t_eFMKTIM_InterruptLineIO f_InterruptLine_e,
                                   t_eFMKTIM_ChnlMeasTrigger f_MeasTrigger_e,
                                   t_float32 f_timerFreqHz_f32,
                                   t_cbFMKTIM_InterruptLine *f_ITChannel_cb)
{
    (void)f_InterruptLine_e;
    (void)f_MeasTrigger_e;
    (void)f_timerFreqHz_f32;
    (void)f_ITChannel_cb;
    return RC_OK;
}

t_eReturnCode FMKTIM_Set_EvntTimerCfg(t_eFMKTIM_InterruptLineEvnt f_EvntITLine_e,
                                      t_float32 f_periodms_f32,
                                      t_cbFMKTIM_InterruptLine f_ITLine_cb)
{
    (void)f_EvntITLine_e;
    g_pcSimFastTimer_s.configured_b = TRUE;
    g_pcSimFastTimer_s.periodMs_f32 = (f_periodms_f32 <= 0.0f) ? 1.0f : f_periodms_f32;
    g_pcSimFastTimer_s.callback_pcb = f_ITLine_cb;
    g_pcSimFastTimer_s.lastTickMs_u32 = PCSIM_GetTickMs();
    return RC_OK;
}

t_eReturnCode FMKTIM_Set_PwmLineValue(t_eFMKTIM_InterruptLineIO f_Itline_e,
                                      t_sFMKTIM_PwmOpe f_PwmOpe_s,
                                      t_uint8 f_maskUpdate_u8)
{
    (void)f_Itline_e;
    (void)f_PwmOpe_s;
    (void)f_maskUpdate_u8;
    return RC_OK;
}

t_eReturnCode FMKTIM_Set_ICLineValue(t_eFMKTIM_InterruptLineIO f_Itline_e,
                                     t_sFMKTIM_ICOpe f_ICOpe_s,
                                     t_uint8 f_mask_u8)
{
    (void)f_Itline_e;
    (void)f_ICOpe_s;
    (void)f_mask_u8;
    return RC_OK;
}

t_eReturnCode FMKTIM_Set_EcdrLineState(t_eFMKTIM_InterruptLineIO f_Itline_e,
                                       t_eFMKTIM_EcdrOpe f_EcdrOpe)
{
    (void)f_Itline_e;
    (void)f_EcdrOpe;
    return RC_OK;
}

t_eReturnCode FMKTIM_Set_EvntLineState(t_eFMKTIM_InterruptLineEvnt f_EvntITLine_e,
                                       t_eFMKTIM_EvntOpe f_EvntOpe)
{
    (void)f_EvntITLine_e;
    if (f_EvntOpe == FMKTIM_EVNT_OPE_START_TIMER)
    {
        g_pcSimFastTimer_s.running_b = TRUE;
        g_pcSimFastTimer_s.lastTickMs_u32 = PCSIM_GetTickMs();
    }
    else if (f_EvntOpe == FMKTIM_EVNT_OPE_STOP_TIMER)
    {
        g_pcSimFastTimer_s.running_b = FALSE;
    }
    else
    {
        return RC_ERROR_PARAM_INVALID;
    }
    return RC_OK;
}

t_eReturnCode FMKTIM_Get_PwmLineValue(t_eFMKTIM_InterruptLineIO f_Itline_e,
                                      t_sFMKTIM_PwmValue *f_PwmValue_ps,
                                      t_uint8 f_mask_u8)
{
    (void)f_Itline_e;
    (void)f_mask_u8;
    if (f_PwmValue_ps == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    f_PwmValue_ps->frequency_f32 = 0.0f;
    f_PwmValue_ps->dutyCycle_u16 = 0U;
    return RC_OK;
}

t_eReturnCode FMKTIM_Get_EcdrLineValue(t_eFMKTIM_InterruptLineIO f_Itline_e,
                                       t_uint32 *f_position_u32,
                                       t_uint32 *f_direction_u32)
{
    (void)f_Itline_e;
    if ((f_position_u32 == NULL) || (f_direction_u32 == NULL))
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_position_u32 = 0U;
    *f_direction_u32 = 0U;
    return RC_OK;
}

t_eReturnCode FMKTIM_Get_ICLineValue(t_eFMKTIM_InterruptLineIO f_Itline_e,
                                     t_sFMKTIM_ICValue *ICValue_ps,
                                     t_uint8 f_mask_u8)
{
    (void)f_Itline_e;
    (void)f_mask_u8;
    if (ICValue_ps == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    ICValue_ps->frequency_f32 = 0.0f;
    ICValue_ps->ARR_Register_u32 = 0U;
    ICValue_ps->CCRxRegister_u16 = 0U;
    return RC_OK;
}

t_eReturnCode FMKTIM_AddInterruptCallback(t_eFMKTIM_InterruptLineIO f_InterruptLine_e,
                                          t_cbFMKTIM_InterruptLine *f_ITLine_cb)
{
    (void)f_InterruptLine_e;
    (void)f_ITLine_cb;
    return RC_OK;
}

t_eReturnCode FMKTIM_Get_LineErrorStatus(t_eFMKTIM_InterruptLineType f_ITLineType_e,
                                         t_uint8 f_IT_line_u8,
                                         t_eFMKTIM_ErrorState *f_chnlErrInfo_pe)
{
    (void)f_ITLineType_e;
    (void)f_IT_line_u8;
    if (f_chnlErrInfo_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_chnlErrInfo_pe = FMKTIM_ERRSTATE_OK;
    return RC_OK;
}

TIM_HandleTypeDef *FMKTIM_PRIVATE_GetHandleTypeDef(t_uint8 f_timer_u8)
{
    (void)f_timer_u8;
    return (TIM_HandleTypeDef *)NULL;
}
