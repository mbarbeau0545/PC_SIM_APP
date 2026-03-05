#ifndef PC_SIM_RUNTIME_H_INCLUDED
#define PC_SIM_RUNTIME_H_INCLUDED

#include "pc_sim_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCSIM_DIM_OR_ONE(x) (((x) > 0U) ? (x) : 1U)
#define PCSIM_CAN_RX_REG_MAX 256U
#define PCSIM_CAN_TX_LOG_MAX 4096U

typedef struct
{
    t_bool configured_b;
    t_bool running_b;
    t_float32 periodMs_f32;
    t_uint32 lastTickMs_u32;
    t_cbFMKTIM_InterruptLine *callback_pcb;
} t_sPCSIM_FastTimer;

typedef struct
{
    t_bool used_b;
    t_eFMKFDCAN_NodeList node_e;
    t_sFMKFDCAN_RxItemEventCfg cfg_s;
} t_sPCSIM_CanRxReg;

typedef struct
{
    t_sPCSIM_CanTxFrame items_as[PCSIM_CAN_TX_LOG_MAX];
    t_uint16 head_u16;
    t_uint16 tail_u16;
    t_uint16 count_u16;
} t_sPCSIM_CanTxLog;

extern t_eCyclicModState g_pcSimCpuState_e;
extern t_eCyclicModState g_pcSimTimState_e;
extern t_eCyclicModState g_pcSimHrtState_e;
extern t_eCyclicModState g_pcSimCdaState_e;
extern t_eCyclicModState g_pcSimIoState_e;
extern t_eCyclicModState g_pcSimCanState_e;
extern t_eCyclicModState g_pcSimSrlState_e;

extern t_float32 g_pcSimAnalogValues_af32[FMKIO_INPUT_SIGANA_NB];
extern t_uint16 g_pcSimPwmDuty_au16[FMKIO_OUTPUT_SIGPWM_NB];
extern t_float32 g_pcSimPwmFreq_af32[FMKIO_OUTPUT_SIGPWM_NB];
extern t_uint16 g_pcSimPwmPulses_au16[FMKIO_OUTPUT_SIGPWM_NB];
extern t_cbFMKIO_PulseEvent *g_pcSimPwmPulseEndCb_apcb[FMKIO_OUTPUT_SIGPWM_NB];
extern t_eFMKIO_DigValue g_pcSimOutDig_ae[FMKIO_OUTPUT_SIGDIG_NB];
extern t_eFMKIO_DigValue g_pcSimInDig_ae[FMKIO_INPUT_SIGDIG_NB];
extern t_float32 g_pcSimInFreq_af32[PCSIM_DIM_OR_ONE(FMKIO_INPUT_SIGFREQ_NB)];
extern t_float32 g_pcSimEncAbs_af32[PCSIM_DIM_OR_ONE(FMKIO_INPUT_ENCODER_NB)];
extern t_float32 g_pcSimEncRel_af32[PCSIM_DIM_OR_ONE(FMKIO_INPUT_ENCODER_NB)];
extern t_float32 g_pcSimEncSpeed_af32[PCSIM_DIM_OR_ONE(FMKIO_INPUT_ENCODER_NB)];
extern t_eFMKIO_EcdrDir g_pcSimEncDir_ae[PCSIM_DIM_OR_ONE(FMKIO_INPUT_ENCODER_NB)];

extern t_sPCSIM_CanRxReg g_pcSimCanRxReg_as[PCSIM_CAN_RX_REG_MAX];
extern t_sPCSIM_CanTxLog g_pcSimCanTxLog_s;
extern t_sPCSIM_CanTxLog g_pcSimCanBrokerTxLog_s;
extern t_sPCSIM_FastTimer g_pcSimFastTimer_s;

void PCSIM_RuntimeInit(void);
void PCSIM_RuntimeStep(void);
void PCSIM_RuntimeNotifyPwmPulsesSet(t_eFMKIO_OutPwmSig signal_e);
t_eReturnCode PCSIM_RuntimeSetEncoderPulseMapping(t_eFMKIO_InEcdrSignals encoder_e,
                                                  t_eFMKIO_OutPwmSig pwm_e,
                                                  t_float32 pulsesPerRev_f32,
                                                  t_eFMKIO_OutDigSig dirDig_e);

t_uint32 PCSIM_GetTickMs(void);

void PCSIM_InternalSleepMs(t_uint32 delayMs_u32);

#ifdef __cplusplus
}
#endif

#endif
