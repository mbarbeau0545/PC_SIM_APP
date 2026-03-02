#include "pc_sim_runtime.h"

t_eReturnCode FMKCDA_Init(void) { return RC_OK; }
t_eReturnCode FMKCDA_Cyclic(void) { return RC_OK; }

t_eReturnCode FMKCDA_GetState(t_eCyclicModState *f_State_pe)
{
    if (f_State_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_State_pe = g_pcSimCdaState_e;
    return RC_OK;
}

t_eReturnCode FMKCDA_SetState(t_eCyclicModState f_State_e)
{
    g_pcSimCdaState_e = f_State_e;
    return RC_OK;
}

t_eReturnCode FMKCDA_Set_AdcChannelCfg(t_eFMKCDA_Adc f_Adc_e, t_eFMKCDA_AdcChannel f_channel_e)
{
    (void)f_Adc_e;
    (void)f_channel_e;
    return RC_OK;
}

t_eReturnCode FMKCDA_Get_AnaChannelMeasure(t_eFMKCDA_Adc f_Adc_e,
                                           t_eFMKCDA_AdcChannel f_channel_e,
                                           t_float32 *f_AnaMeasure_pf32)
{
    (void)f_Adc_e;
    (void)f_channel_e;
    if (f_AnaMeasure_pf32 == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_AnaMeasure_pf32 = 0.0f;
    return RC_OK;
}

t_eReturnCode FMKCDA_Get_AnaInternSnsMeasure(t_eFMKCDA_AdcInternSns f_AdcInternSns_e,
                                             t_float32 *f_AnaMeasure_pf32)
{
    (void)f_AdcInternSns_e;
    if (f_AnaMeasure_pf32 == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_AnaMeasure_pf32 = 0.0f;
    return RC_OK;
}

t_eReturnCode FMKCDA_Get_AdcError(t_eFMKCDA_Adc f_adc_e, t_uint16 *f_chnlErrInfo_pu16)
{
    (void)f_adc_e;
    if (f_chnlErrInfo_pu16 == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_chnlErrInfo_pu16 = 0U;
    return RC_OK;
}

ADC_HandleTypeDef *FMKCDA_PRIVATE_GetHandleTypeDef(t_eFMKCDA_Adc f_adc_e)
{
    (void)f_adc_e;
    return (ADC_HandleTypeDef *)NULL;
}
