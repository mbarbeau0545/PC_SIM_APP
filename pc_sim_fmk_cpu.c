#include "pc_sim_runtime.h"
#include "FMK_HAL/FMK_CPU/Src/FMK_CPU.h"

t_eReturnCode FMKCPU_Init(void) { return RC_OK; }
t_eReturnCode FMKCPU_Cyclic(void) { return RC_OK; }

t_eReturnCode FMKCPU_GetState(t_eCyclicModState *f_State_pe)
{
    if (f_State_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_State_pe = g_pcSimCpuState_e;
    return RC_OK;
}

t_eReturnCode FMKCPU_SetState(t_eCyclicModState f_State_e)
{
    g_pcSimCpuState_e = f_State_e;
    return RC_OK;
}

t_eReturnCode FMKCPU_Set_SysClockCfg(t_eFMKCPU_CoreClockSpeed f_SystemCoreFreq_e)
{
    (void)f_SystemCoreFreq_e;
    return RC_OK;
}

void FMKCPU_Set_Delay(t_uint32 f_delayms_u32)
{
    PCSIM_InternalSleepMs(f_delayms_u32);
}

void FMKCPU_GetTick(t_uint32 *f_tickms_pu32)
{
    if (f_tickms_pu32 != NULL)
    {
        *f_tickms_pu32 = PCSIM_GetTickMs();
    }
}

t_eReturnCode FMKCPU_Set_HardwareInit(void) { return RC_OK; }

t_eReturnCode FMKCPU_Set_NVICState(t_eFMKCPU_IRQNType f_IRQN_e, t_eFMKCPU_NVIC_Ope f_OpeState_e)
{
    (void)f_IRQN_e;
    (void)f_OpeState_e;
    return RC_OK;
}

t_eReturnCode FMKCPU_Set_HwClock(t_eFMKCPU_ClockPort f_clkPort_e, t_eFMKCPU_ClockPortOpe f_OpeState_e)
{
    (void)f_clkPort_e;
    (void)f_OpeState_e;
    return RC_OK;
}

t_eReturnCode FMKCPU_Set_WwdgCfg(t_eFMKCPu_WwdgResetPeriod f_period_e)
{
    (void)f_period_e;
    return RC_OK;
}

void FMKCPU_RearmWwdg(void) {}

t_eReturnCode FMKCPU_RqstDmaInit(t_eFMKCPU_DmaRqst f_DmaRqstType,
                                 t_eFMKCPU_DmaType f_dmaType_e,
                                 void *f_ModuleHandle_pv)
{
    (void)f_DmaRqstType;
    (void)f_dmaType_e;
    (void)f_ModuleHandle_pv;
    return RC_OK;
}

t_eReturnCode FMKCPU_GetOscRccSrc(t_eFMKCPU_ClockPort f_clockPort_e,
                                  t_eFMKCPU_SysClkOsc *f_ClkOsc_pe)
{
    (void)f_clockPort_e;
    if (f_ClkOsc_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_ClkOsc_pe = (t_eFMKCPU_SysClkOsc)0;
    return RC_OK;
}

t_eReturnCode FMKCPU_GetSysClkValue(t_eFMKCPU_SysClkOsc f_ClkOsc_e,
                                    t_uint16 *f_OscValueMHz_pu16)
{
    (void)f_ClkOsc_e;
    if (f_OscValueMHz_pu16 == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_OscValueMHz_pu16 = 128U;
    return RC_OK;
}

DMA_HandleTypeDef *FMKCPU_PRIVATE_GetHandleTypeDef(t_eFMKCPU_DmaController f_dmaCtrl_e,
                                                    t_eFMKCPU_DmaChnl f_chnle_e)
{
    (void)f_dmaCtrl_e;
    (void)f_chnle_e;
    return (DMA_HandleTypeDef *)NULL;
}
