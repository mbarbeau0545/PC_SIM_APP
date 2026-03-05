#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef LPTR
#undef LPTR
#endif
#ifdef ERROR
#undef ERROR
#endif
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#else
#include <time.h>
#include <unistd.h>
#endif

#include "pc_sim_runtime.h"

#include <stdint.h>
#include <math.h>

t_eCyclicModState g_pcSimCpuState_e = STATE_CYCLIC_OPE;
t_eCyclicModState g_pcSimTimState_e = STATE_CYCLIC_OPE;
t_eCyclicModState g_pcSimHrtState_e = STATE_CYCLIC_OPE;
t_eCyclicModState g_pcSimCdaState_e = STATE_CYCLIC_OPE;
t_eCyclicModState g_pcSimIoState_e = STATE_CYCLIC_OPE;
t_eCyclicModState g_pcSimCanState_e = STATE_CYCLIC_OPE;
t_eCyclicModState g_pcSimSrlState_e = STATE_CYCLIC_OPE;

t_float32 g_pcSimAnalogValues_af32[FMKIO_INPUT_SIGANA_NB];
t_uint16 g_pcSimPwmDuty_au16[FMKIO_OUTPUT_SIGPWM_NB];
t_float32 g_pcSimPwmFreq_af32[FMKIO_OUTPUT_SIGPWM_NB];
t_uint32 g_pcSimPwmPulses_au32[FMKIO_OUTPUT_SIGPWM_NB];
t_cbFMKIO_PulseEvent *g_pcSimPwmPulseEndCb_apcb[FMKIO_OUTPUT_SIGPWM_NB];
t_cbFMKIO_EventFunc *g_pcSimInEvntCb_apcb[PCSIM_DIM_OR_ONE(FMKIO_INPUT_SIGEVNT_NB)];
t_eFMKIO_SigTrigCptr g_pcSimInEvntTrigger_ae[PCSIM_DIM_OR_ONE(FMKIO_INPUT_SIGEVNT_NB)];
t_bool g_pcSimInEvntConfigured_ab[PCSIM_DIM_OR_ONE(FMKIO_INPUT_SIGEVNT_NB)];
t_eFMKIO_DigValue g_pcSimOutDig_ae[FMKIO_OUTPUT_SIGDIG_NB];
t_eFMKIO_DigValue g_pcSimInDig_ae[FMKIO_INPUT_SIGDIG_NB];
t_float32 g_pcSimInFreq_af32[PCSIM_DIM_OR_ONE(FMKIO_INPUT_SIGFREQ_NB)];
t_float32 g_pcSimEncAbs_af32[PCSIM_DIM_OR_ONE(FMKIO_INPUT_ENCODER_NB)];
t_float32 g_pcSimEncRel_af32[PCSIM_DIM_OR_ONE(FMKIO_INPUT_ENCODER_NB)];
t_float32 g_pcSimEncSpeed_af32[PCSIM_DIM_OR_ONE(FMKIO_INPUT_ENCODER_NB)];
t_eFMKIO_EcdrDir g_pcSimEncDir_ae[PCSIM_DIM_OR_ONE(FMKIO_INPUT_ENCODER_NB)];

t_sPCSIM_CanRxReg g_pcSimCanRxReg_as[PCSIM_CAN_RX_REG_MAX];
t_sPCSIM_CanTxLog g_pcSimCanTxLog_s;
t_sPCSIM_CanTxLog g_pcSimCanBrokerTxLog_s;
t_sPCSIM_FastTimer g_pcSimFastTimer_s;

static uint64_t g_startTickMs_u64;
static t_uint32 g_lastRuntimeTickMs_u32;
#define PCSIM_PWM_SLOT_NB (((FMKIO_OUTPUT_SIGPWM_NB) > 0U) ? (FMKIO_OUTPUT_SIGPWM_NB) : 1U)
#define PCSIM_ENCODER_SLOT_NB (((FMKIO_INPUT_ENCODER_NB) > 0U) ? (FMKIO_INPUT_ENCODER_NB) : 1U)
static t_float32 g_pcSimPwmPulseFrac_af32[PCSIM_DIM_OR_ONE(FMKIO_OUTPUT_SIGPWM_NB)];
static t_uint16 g_pcSimEncLinkedPwm_au16[PCSIM_ENCODER_SLOT_NB];
static t_float32 g_pcSimEncPulsePerRev_af32[PCSIM_ENCODER_SLOT_NB];
static t_uint16 g_pcSimEncLinkedDirDig_au16[PCSIM_ENCODER_SLOT_NB];

#define PCSIM_PI_F 3.1415926f
#define PCSIM_ENCODER_PULSE_PER_REV_F 3200.0f
#define PCSIM_PI_MRAD_F (PCSIM_PI_F * 1000.0f)
#define PCSIM_2PI_MRAD_F (2.0f * PCSIM_PI_MRAD_F)

static t_float32 s_WrapToPi_mrad(t_float32 x_mrad_f32)
{
    while (x_mrad_f32 >= PCSIM_PI_MRAD_F)
    {
        x_mrad_f32 -= PCSIM_2PI_MRAD_F;
    }
    while (x_mrad_f32 < -PCSIM_PI_MRAD_F)
    {
        x_mrad_f32 += PCSIM_2PI_MRAD_F;
    }
    return x_mrad_f32;
}

static t_uint32 s_getTickMs(void)
{
#if defined(_WIN32)
    return (t_uint32)(((uint64_t)GetTickCount()) - g_startTickMs_u64);
#else
    struct timespec now_s;
    clock_gettime(CLOCK_MONOTONIC, &now_s);
    return (t_uint32)((((uint64_t)now_s.tv_sec) * 1000ULL) +
                      (((uint64_t)now_s.tv_nsec) / 1000000ULL) -
                      g_startTickMs_u64);
#endif
}

t_uint32 PCSIM_GetTickMs(void)
{
    return s_getTickMs();
}

void PCSIM_InternalSleepMs(t_uint32 delayMs_u32)
{
#if defined(_WIN32)
    Sleep(delayMs_u32);
#else
    usleep((useconds_t)(delayMs_u32 * 1000U));
#endif
}

static void s_stepPulseDrivenEncoder(t_float32 dtSec_f32)
{
    t_uint16 pwm_u16;
    t_uint16 enc_u16;
    t_uint32 prevPulses_u32;
    t_float32 absFreq_f32;
    t_float32 pulsesToEmit_f32;
    t_uint32 emitted_u32;
    t_float32 deltaPos_mrad_f32;
    t_float32 ppr_f32;
    t_float32 mradPerPulse_f32;
    t_float32 sign_f32;
    t_uint16 dirDig_u16;

    if (dtSec_f32 <= 0.0f)
    {
        return;
    }

    for (enc_u16 = 0U; enc_u16 < PCSIM_ENCODER_SLOT_NB; enc_u16++)
    {
        g_pcSimEncSpeed_af32[enc_u16] = 0.0f;
    }

    for (pwm_u16 = 0U; pwm_u16 < PCSIM_PWM_SLOT_NB; pwm_u16++)
    {
        prevPulses_u32 = g_pcSimPwmPulses_au32[pwm_u16];
        if (prevPulses_u32 == 0U)
        {
            g_pcSimPwmPulseFrac_af32[pwm_u16] = 0.0f;
            continue;
        }

        absFreq_f32 = (t_float32)fabs((double)g_pcSimPwmFreq_af32[pwm_u16]);
        if (absFreq_f32 <= 0.01f)
        {
            continue;
        }

        pulsesToEmit_f32 = (absFreq_f32 * dtSec_f32) + g_pcSimPwmPulseFrac_af32[pwm_u16];
        emitted_u32 = (t_uint32)pulsesToEmit_f32;
        if (emitted_u32 > prevPulses_u32)
        {
            emitted_u32 = prevPulses_u32;
        }
        g_pcSimPwmPulseFrac_af32[pwm_u16] = pulsesToEmit_f32 - (t_float32)emitted_u32;

        if (emitted_u32 == 0U)
        {
            continue;
        }

        g_pcSimPwmPulses_au32[pwm_u16] = prevPulses_u32 - emitted_u32;
        for (enc_u16 = 0U; enc_u16 < PCSIM_ENCODER_SLOT_NB; enc_u16++)
        {
            if (g_pcSimEncLinkedPwm_au16[enc_u16] != pwm_u16)
            {
                continue;
            }

            ppr_f32 = g_pcSimEncPulsePerRev_af32[enc_u16];
            if (ppr_f32 <= 0.01f)
            {
                continue;
            }
            mradPerPulse_f32 = (2.0f * PCSIM_PI_F * 1000.0f) / ppr_f32;
            dirDig_u16 = g_pcSimEncLinkedDirDig_au16[enc_u16];
            if (dirDig_u16 < FMKIO_OUTPUT_SIGDIG_NB)
            {
                sign_f32 = (g_pcSimOutDig_ae[dirDig_u16] == FMKIO_DIG_VALUE_HIGH) ? 1.0f : -1.0f;
            }
            else
            {
                sign_f32 = (g_pcSimPwmFreq_af32[pwm_u16] >= 0.0f) ? 1.0f : -1.0f;
            }

            if (sign_f32 > 0.0f)
            {
                g_pcSimEncDir_ae[enc_u16] = FMKIO_ENCODER_DIR_FORWARD;
                deltaPos_mrad_f32 = (t_float32)emitted_u32 * mradPerPulse_f32;
                g_pcSimEncSpeed_af32[enc_u16] += absFreq_f32 * mradPerPulse_f32;
            }
            else
            {
                g_pcSimEncDir_ae[enc_u16] = FMKIO_ENCODER_DIR_BACKWARD;
                deltaPos_mrad_f32 = (t_float32)emitted_u32 * (-mradPerPulse_f32);
                g_pcSimEncSpeed_af32[enc_u16] -= absFreq_f32 * mradPerPulse_f32;
            }

            g_pcSimEncRel_af32[enc_u16] += deltaPos_mrad_f32;
            g_pcSimEncAbs_af32[enc_u16] = s_WrapToPi_mrad(g_pcSimEncRel_af32[enc_u16]);
        }

        /* Emulate hardware pulse finished interrupt callback. */
        if ((g_pcSimPwmPulses_au32[pwm_u16] == 0U)
            && (g_pcSimPwmPulseEndCb_apcb[pwm_u16] != (t_cbFMKIO_PulseEvent *)NULL_FUNCTION))
        {
            g_pcSimPwmPulseEndCb_apcb[pwm_u16]((t_eFMKIO_OutPwmSig)pwm_u16);
        }
    }
}

void PCSIM_RuntimeInit(void)
{
    t_uint16 idx_u16;
#if defined(_WIN32)
    g_startTickMs_u64 = (uint64_t)GetTickCount();
#else
    struct timespec now_s;
    clock_gettime(CLOCK_MONOTONIC, &now_s);
    g_startTickMs_u64 = (((uint64_t)now_s.tv_sec) * 1000ULL) +
                        (((uint64_t)now_s.tv_nsec) / 1000000ULL);
#endif

    for (idx_u16 = 0U; idx_u16 < FMKIO_INPUT_SIGANA_NB; idx_u16++)
    {
        g_pcSimAnalogValues_af32[idx_u16] = 0.0f;
    }
    for (idx_u16 = 0U; idx_u16 < FMKIO_OUTPUT_SIGPWM_NB; idx_u16++)
    {
        g_pcSimPwmDuty_au16[idx_u16] = 0U;
        g_pcSimPwmFreq_af32[idx_u16] = 0.0f;
        g_pcSimPwmPulses_au32[idx_u16] = 0U;
        g_pcSimPwmPulseEndCb_apcb[idx_u16] = (t_cbFMKIO_PulseEvent *)NULL_FUNCTION;
    }
    for (idx_u16 = 0U; idx_u16 < PCSIM_DIM_OR_ONE(FMKIO_INPUT_SIGEVNT_NB); idx_u16++)
    {
        g_pcSimInEvntCb_apcb[idx_u16] = (t_cbFMKIO_EventFunc *)NULL_FUNCTION;
        g_pcSimInEvntTrigger_ae[idx_u16] = FMKIO_STC_RISING_EDGE;
        g_pcSimInEvntConfigured_ab[idx_u16] = FALSE;
    }
    for (idx_u16 = 0U; idx_u16 < FMKIO_OUTPUT_SIGDIG_NB; idx_u16++)
    {
        g_pcSimOutDig_ae[idx_u16] = FMKIO_DIG_VALUE_LOW;
    }
    for (idx_u16 = 0U; idx_u16 < FMKIO_INPUT_SIGDIG_NB; idx_u16++)
    {
        g_pcSimInDig_ae[idx_u16] = FMKIO_DIG_VALUE_LOW;
    }
    for (idx_u16 = 0U; idx_u16 < PCSIM_CAN_RX_REG_MAX; idx_u16++)
    {
        g_pcSimCanRxReg_as[idx_u16].used_b = FALSE;
        g_pcSimCanRxReg_as[idx_u16].node_e = (t_eFMKFDCAN_NodeList)0U;
    }
    for (idx_u16 = 0U; idx_u16 < PCSIM_DIM_OR_ONE(FMKIO_INPUT_SIGFREQ_NB); idx_u16++)
    {
        g_pcSimInFreq_af32[idx_u16] = 0.0f;
    }
    for (idx_u16 = 0U; idx_u16 < PCSIM_DIM_OR_ONE(FMKIO_INPUT_ENCODER_NB); idx_u16++)
    {
        g_pcSimEncAbs_af32[idx_u16] = 0.0f;
        g_pcSimEncRel_af32[idx_u16] = 0.0f;
        g_pcSimEncSpeed_af32[idx_u16] = 0.0f;
        g_pcSimEncDir_ae[idx_u16] = FMKIO_ENCODER_DIR_FORWARD;
    }
    g_pcSimCanTxLog_s.head_u16 = 0U;
    g_pcSimCanTxLog_s.tail_u16 = 0U;
    g_pcSimCanTxLog_s.count_u16 = 0U;
    g_pcSimCanBrokerTxLog_s.head_u16 = 0U;
    g_pcSimCanBrokerTxLog_s.tail_u16 = 0U;
    g_pcSimCanBrokerTxLog_s.count_u16 = 0U;
    g_lastRuntimeTickMs_u32 = 0U;
    for (idx_u16 = 0U; idx_u16 < PCSIM_PWM_SLOT_NB; idx_u16++)
    {
        g_pcSimPwmPulseFrac_af32[idx_u16] = 0.0f;
    }
    for (idx_u16 = 0U; idx_u16 < PCSIM_ENCODER_SLOT_NB; idx_u16++)
    {
        g_pcSimEncPulsePerRev_af32[idx_u16] = PCSIM_ENCODER_PULSE_PER_REV_F;
        g_pcSimEncLinkedPwm_au16[idx_u16] = (t_uint16)(idx_u16 % PCSIM_PWM_SLOT_NB);
        g_pcSimEncLinkedDirDig_au16[idx_u16] = (t_uint16)(idx_u16 % PCSIM_DIM_OR_ONE(FMKIO_OUTPUT_SIGDIG_NB));
    }

    g_pcSimFastTimer_s.configured_b = FALSE;
    g_pcSimFastTimer_s.running_b = FALSE;
    g_pcSimFastTimer_s.periodMs_f32 = 0.0f;
    g_pcSimFastTimer_s.lastTickMs_u32 = 0U;
    g_pcSimFastTimer_s.callback_pcb = NULL_FUNCTION;
}

t_eReturnCode PCSIM_RuntimeSetEncoderPulseMapping(t_eFMKIO_InEcdrSignals encoder_e,
                                                  t_eFMKIO_OutPwmSig pwm_e,
                                                  t_float32 pulsesPerRev_f32,
                                                  t_eFMKIO_OutDigSig dirDig_e)
{
    if ((encoder_e >= FMKIO_INPUT_ENCODER_NB)
        || (pwm_e >= FMKIO_OUTPUT_SIGPWM_NB)
        || (pulsesPerRev_f32 <= 0.01f))
    {
        return RC_ERROR_PARAM_INVALID;
    }

    g_pcSimEncLinkedPwm_au16[(t_uint16)encoder_e] = (t_uint16)pwm_e;
    g_pcSimEncPulsePerRev_af32[(t_uint16)encoder_e] = pulsesPerRev_f32;
    g_pcSimEncLinkedDirDig_au16[(t_uint16)encoder_e] = (t_uint16)dirDig_e;
    return RC_OK;
}

void PCSIM_RuntimeNotifyPwmPulsesSet(t_eFMKIO_OutPwmSig signal_e)
{
    if ((t_uint16)signal_e < PCSIM_PWM_SLOT_NB)
    {
        g_pcSimPwmPulseFrac_af32[(t_uint16)signal_e] = 0.0f;
    }
}

void PCSIM_RuntimeStep(void)
{
    t_uint32 nowTick_u32 = s_getTickMs();
    t_float32 dtSec_f32;

    if (g_lastRuntimeTickMs_u32 == 0U)
    {
        g_lastRuntimeTickMs_u32 = nowTick_u32;
    }
    dtSec_f32 = (t_float32)(nowTick_u32 - g_lastRuntimeTickMs_u32) / 1000.0f;
    g_lastRuntimeTickMs_u32 = nowTick_u32;

    s_stepPulseDrivenEncoder(dtSec_f32);
    (void)FMKTIM_Cyclic();
}
