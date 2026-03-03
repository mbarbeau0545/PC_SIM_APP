#if defined(__has_include)
#if __has_include("2_DRV/CL42T/Src/CL42T.h")
#include "2_DRV/CL42T/Src/CL42T.h"
#define PCSIM_CL42T_HEADER_FOUND 1
#elif __has_include("CL42T/Src/CL42T.h")
#include "CL42T/Src/CL42T.h"
#define PCSIM_CL42T_HEADER_FOUND 1
#endif
#endif

#if !defined(PCSIM_CL42T_HEADER_FOUND)
#include "TypeCommon.h"
#include "FMK_HAL/FMK_IO/Src/FMK_IO.h"

/* Fallback minimal CL42T interface for standalone PCSIM builds. */
typedef enum
{
    CL42T_MOTOR_1 = 0,
    CL42T_MOTOR_2,
    CL42T_MOTOR_3,
    CL42T_MOTOR_4,
    CL42T_MOTOR_5,
    CL42T_MOTOR_6,
    CL42T_MOTOR_7,
    CL42T_MOTOR_8,
    CL42T_MOTOR_9,
    CL42T_MOTOR_10,
    CL42T_MOTOR_NB
} t_eCL42T_MotorId;

typedef enum
{
    CL42T_MOTOR_DIRECTION_CW = 0,
    CL42T_MOTOR_DIRECTION_CCW,
    CL42T_MOTOR_DIRECTION_NB
} t_eCL42T_MotorDirection;

typedef enum
{
    CL42T_MOTOR_STATE_OFF = 0,
    CL42T_MOTOR_STATE_ON,
    CL42T_MOTOR_STATE_NB
} t_eCL42T_MotorState;

typedef enum
{
    CL42T_DIAGNOSTIC_OK = 0,
    CL42T_DIAGNOSTIC_PRESENTS,
    CL42T_DIAGNOSTIC_OVER_CURRENT,
    CL42T_DIAGNOSTIC_OVER_VOLTAGE,
    CL42T_DIAGNOSTIC_CHIP_ERROR,
    CL42T_DIAGNOSTIC_LOCK_MOTOR_SHAFT,
    CL42T_DIAGNOSTIC_AUTO_TUNNING,
    CL42T_DIAGNOSTIC_EEPROM,
    CL42T_DIAGNOSTIC_POSITION,
    CL42T_DIAGNOSTIC_PCB_BOARD,
    CL42T_DIAGNOSTIC_SIGNAL_PULSE,
    CL42T_DIAGNOSTIC_SIGNAL_FREQ,
    CL42T_DIAGNOSTIC_PULSE_INFINITE,
    CL42T_DIAGNOSTIC_NB
} t_eCL42T_DiagError;

typedef enum
{
    CL42T_BITFIELD_MOTOR_ENABLE = 0,
    CL42T_BITFIELD_MOTOR_ON,
    CL42T_BITFILED_MOTOR_DIR,
    CL42T_BITFIELD_IN_DEAD_TIME,
    CL42T_BITFIELD_TRIG_ENDSTOP_CW,
    CL42T_BITFIELD_TRIG_ENDSTOP_CCW,
    CL42T_BITFIELD_NB
} t_eCL42T_BitfieldInfo;

typedef struct
{
    t_eFMKIO_InEvntSig EndStopSignal_e;
    t_eFMKIO_PullMode PullMode_e;
    t_eFMKIO_SigTrigCptr triggerEvnt_e;
    t_uint16 debuncValue_u16;
} t_sCL42T_EndStopignalCfg;

typedef struct
{
    t_eFMKIO_OutPwmSig PulseSignal_e;
    t_sFMKIO_PwmControlPrm pwmCtrlPrm_s;
    t_sFMKIO_PwmWaveformCfg pwmWaveForm_s;
} t_sCL42T_PwmSignalCfg;

typedef struct
{
    t_sCL42T_PwmSignalCfg PulseSigCfg_s;
    t_eFMKIO_OutDigSig DirSignal_e;
    t_eFMKIO_OutDigSig StateSignal_e;
    t_eFMKIO_InFreqSig DiagSignal_e;
    t_sCL42T_EndStopignalCfg EndStopSigCW_s;
    t_sCL42T_EndStopignalCfg EndStopSigCCW_s;
} t_sCL42T_MotorSigCfg;

typedef void (t_cbCL42T_Diagnostic)(t_eCL42T_MotorId f_MotorID_e, t_eCL42T_DiagError f_defaultInfo_e);
typedef void (t_cbCL42T_PulseDropped)(t_eCL42T_MotorId f_MotorID_e, t_uint16 f_pulseDropped_u16, t_eCL42T_MotorDirection f_direction_e);

typedef struct
{
    t_sint32 nbPulses_s32;
    t_float32 frequency_f32;
    t_uint32 triggerTimer_u32;
} t_sCL42T_SetMotorValue;
#endif

typedef struct
{
    t_bool configured_b;
    t_bool enableDeadtime_b;
    t_uint16 maskInfo_u16;
    t_float32 speed_f32;
    t_cbCL42T_Diagnostic *diagCb_pcb;
    t_cbCL42T_PulseDropped *pulseDroppedCb_pcb;
} t_sPCSIM_CL42T_MotorCtx;

static t_bool g_pcSimCl42tInit_b = FALSE;
static t_eCyclicModState g_pcSimCl42tState_e = STATE_CYCLIC_PREOPE;
static t_sPCSIM_CL42T_MotorCtx g_pcSimCl42tMotors_as[CL42T_MOTOR_NB];

static t_bool s_isMotorValid(t_eCL42T_MotorId motorId_e)
{
    return (motorId_e < CL42T_MOTOR_NB) ? TRUE : FALSE;
}

t_eReturnCode CL42T_Init(void)
{
    t_uint8 idx_u8;

    g_pcSimCl42tInit_b = TRUE;
    g_pcSimCl42tState_e = STATE_CYCLIC_PREOPE;

    for (idx_u8 = 0U; idx_u8 < (t_uint8)CL42T_MOTOR_NB; idx_u8++)
    {
        g_pcSimCl42tMotors_as[idx_u8].configured_b = FALSE;
        g_pcSimCl42tMotors_as[idx_u8].enableDeadtime_b = FALSE;
        g_pcSimCl42tMotors_as[idx_u8].maskInfo_u16 = 0U;
        g_pcSimCl42tMotors_as[idx_u8].speed_f32 = 0.0f;
        g_pcSimCl42tMotors_as[idx_u8].diagCb_pcb = NULL_FUNCTION;
        g_pcSimCl42tMotors_as[idx_u8].pulseDroppedCb_pcb = NULL_FUNCTION;
    }

    return RC_OK;
}

t_eReturnCode CL42T_Cyclic(void)
{
    if (g_pcSimCl42tInit_b == FALSE)
    {
        return RC_ERROR_MODULE_NOT_INITIALIZED;
    }

    if (g_pcSimCl42tState_e == STATE_CYCLIC_PREOPE)
    {
        g_pcSimCl42tState_e = STATE_CYCLIC_OPE;
    }

    return RC_OK;
}

t_eReturnCode CL42T_GetState(t_eCyclicModState *f_State_pe)
{
    if (f_State_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }

    *f_State_pe = g_pcSimCl42tState_e;
    return RC_OK;
}

t_eReturnCode CL42T_SetState(t_eCyclicModState f_State_e)
{
    g_pcSimCl42tState_e = f_State_e;
    return RC_OK;
}

t_eReturnCode CL42T_AddMotorConfiguration(t_eCL42T_MotorId f_motorId_e,
                                          t_sCL42T_MotorSigCfg f_MotorCfg_s,
                                          t_bool f_enableDeadtime_b,
                                          t_cbCL42T_Diagnostic *f_diagEvnt_pcb,
                                          t_cbCL42T_PulseDropped *f_pulseDropped_pcb)
{
    t_sPCSIM_CL42T_MotorCtx *motor_ps;

    (void)f_MotorCfg_s;

    if (s_isMotorValid(f_motorId_e) == FALSE)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (g_pcSimCl42tInit_b == FALSE)
    {
        return RC_ERROR_MODULE_NOT_INITIALIZED;
    }

    motor_ps = &g_pcSimCl42tMotors_as[f_motorId_e];
    if (motor_ps->configured_b == TRUE)
    {
        return RC_ERROR_ALREADY_CONFIGURED;
    }

    motor_ps->configured_b = TRUE;
    motor_ps->enableDeadtime_b = f_enableDeadtime_b;
    motor_ps->diagCb_pcb = f_diagEvnt_pcb;
    motor_ps->pulseDroppedCb_pcb = f_pulseDropped_pcb;
    SETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_MOTOR_ENABLE);
    RESETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_MOTOR_ON);
    RESETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_IN_DEAD_TIME);
    RESETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_TRIG_ENDSTOP_CW);
    RESETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_TRIG_ENDSTOP_CCW);
    motor_ps->speed_f32 = 0.0f;

    return RC_OK;
}

t_eReturnCode CL42T_SetMotorSigValue(t_eCL42T_MotorId f_motorId_e,
                                     t_sCL42T_SetMotorValue f_MotorValue_s)
{
    t_sPCSIM_CL42T_MotorCtx *motor_ps;

    (void)f_MotorValue_s.triggerTimer_u32;

    if (s_isMotorValid(f_motorId_e) == FALSE)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (g_pcSimCl42tInit_b == FALSE)
    {
        return RC_ERROR_MODULE_NOT_INITIALIZED;
    }
    if (g_pcSimCl42tState_e != STATE_CYCLIC_OPE)
    {
        return RC_WARNING_BUSY;
    }

    motor_ps = &g_pcSimCl42tMotors_as[f_motorId_e];
    if (motor_ps->configured_b == FALSE)
    {
        return RC_ERROR_INSTANCE_NOT_INITIALIZED;
    }
    if (GETBIT(motor_ps->maskInfo_u16, CL42T_BITFIELD_MOTOR_ENABLE) == BIT_IS_RESET_16B)
    {
        return RC_WARNING_NOT_ALLOWED;
    }

    if (f_MotorValue_s.nbPulses_s32 == 0)
    {
        motor_ps->speed_f32 = 0.0f;
        RESETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_MOTOR_ON);
        return RC_OK;
    }

    if (f_MotorValue_s.nbPulses_s32 > 0)
    {
        RESETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFILED_MOTOR_DIR);
    }
    else
    {
        SETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFILED_MOTOR_DIR);
    }

    motor_ps->speed_f32 = (f_MotorValue_s.frequency_f32 < 0.0f)
                              ? -f_MotorValue_s.frequency_f32
                              : f_MotorValue_s.frequency_f32;
    SETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_MOTOR_ON);
    RESETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_TRIG_ENDSTOP_CW);
    RESETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_TRIG_ENDSTOP_CCW);

    return RC_OK;
}

t_eReturnCode CL42T_GetMotorInfo(t_eCL42T_MotorId f_motorId_e,
                                 t_uint16 *f_MotorStsInfo_pu16)
{
    if (f_MotorStsInfo_pu16 == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    if (s_isMotorValid(f_motorId_e) == FALSE)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (g_pcSimCl42tInit_b == FALSE)
    {
        return RC_ERROR_MODULE_NOT_INITIALIZED;
    }
    if (g_pcSimCl42tMotors_as[f_motorId_e].configured_b == FALSE)
    {
        return RC_ERROR_INSTANCE_NOT_INITIALIZED;
    }

    *f_MotorStsInfo_pu16 = g_pcSimCl42tMotors_as[f_motorId_e].maskInfo_u16;
    return RC_OK;
}

t_eReturnCode CL42T_GetMotorSpeed(t_eCL42T_MotorId f_motorId_e,
                                  t_float32 *f_motorSpeed_pf32)
{
    if (f_motorSpeed_pf32 == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    if (s_isMotorValid(f_motorId_e) == FALSE)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (g_pcSimCl42tInit_b == FALSE)
    {
        return RC_ERROR_MODULE_NOT_INITIALIZED;
    }
    if (g_pcSimCl42tMotors_as[f_motorId_e].configured_b == FALSE)
    {
        return RC_ERROR_INSTANCE_NOT_INITIALIZED;
    }

    *f_motorSpeed_pf32 = g_pcSimCl42tMotors_as[f_motorId_e].speed_f32;
    return RC_OK;
}

t_eReturnCode CL42T_SetMotorState(t_eCL42T_MotorId f_motorId_e,
                                  t_eCL42T_MotorState f_state_e,
                                  t_bool f_isEmergencyStop_b)
{
    t_sPCSIM_CL42T_MotorCtx *motor_ps;

    (void)f_isEmergencyStop_b;

    if (s_isMotorValid(f_motorId_e) == FALSE)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if (g_pcSimCl42tInit_b == FALSE)
    {
        return RC_ERROR_MODULE_NOT_INITIALIZED;
    }

    motor_ps = &g_pcSimCl42tMotors_as[f_motorId_e];
    if (motor_ps->configured_b == FALSE)
    {
        return RC_ERROR_INSTANCE_NOT_INITIALIZED;
    }

    if (f_state_e == CL42T_MOTOR_STATE_ON)
    {
        SETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_MOTOR_ENABLE);
    }
    else if (f_state_e == CL42T_MOTOR_STATE_OFF)
    {
        RESETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_MOTOR_ENABLE);
        RESETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_MOTOR_ON);
        motor_ps->speed_f32 = 0.0f;
    }
    else
    {
        return RC_ERROR_PARAM_INVALID;
    }

    return RC_OK;
}

void CL42T_Test_SetPerturb(t_eCL42T_MotorId f_motor_e, t_bool isEndStopCW)
{
    t_sPCSIM_CL42T_MotorCtx *motor_ps;

    if (s_isMotorValid(f_motor_e) == FALSE)
    {
        return;
    }
    motor_ps = &g_pcSimCl42tMotors_as[f_motor_e];
    if (motor_ps->configured_b == FALSE)
    {
        return;
    }

    if (isEndStopCW == TRUE)
    {
        SETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_TRIG_ENDSTOP_CW);
        RESETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_TRIG_ENDSTOP_CCW);
    }
    else
    {
        SETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_TRIG_ENDSTOP_CCW);
        RESETBIT_16B(motor_ps->maskInfo_u16, CL42T_BITFIELD_TRIG_ENDSTOP_CW);
    }
}
