#ifndef PCS_SIM_API_H_INCLUDED
#define PCS_SIM_API_H_INCLUDED

#include "TypeCommon.h"
#include "FMK_HAL/FMK_IO/Src/FMK_IO.h"
#include "FMK_HAL/FMK_CAN/Src/FMK_FDCAN.h"
#include "FMK_HAL/FMK_SRL/Src/FMK_SRL.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    t_uint32 timeStamp_u32;
    t_eFMKFDCAN_NodeList node_e;
    t_uint32 identifier_u32;
    t_eFMKFDCAN_IdentifierType idType_e;
    t_eFMKFDCAN_FramePurpose purpose_e;
    t_eFMKFDCAN_DataLength dlc_e;
    t_uint8 data_au8[64];
} t_sPCSIM_CanTxFrame;

void PCSIM_RuntimeInit(void);
void PCSIM_RuntimeStep(void);

/* External control API */
t_eReturnCode PCSIM_SetAnalogInput(t_eFMKIO_InAnaSig signal, t_float32 value);
t_eReturnCode PCSIM_GetAnalogInput(t_eFMKIO_InAnaSig signal, t_float32 *value);
t_eReturnCode PCSIM_SetInputDigital(t_eFMKIO_InDigSig signal, t_eFMKIO_DigValue value);
t_eReturnCode PCSIM_GetInputDigital(t_eFMKIO_InDigSig signal, t_eFMKIO_DigValue *value);
t_eReturnCode PCSIM_SetInputFrequency(t_eFMKIO_InFreqSig signal, t_float32 value);
t_eReturnCode PCSIM_GetInputFrequency(t_eFMKIO_InFreqSig signal, t_float32 *value);
t_eReturnCode PCSIM_SetEncoderPosition(t_eFMKIO_InEcdrSignals signal, t_float32 absolute, t_float32 relative);
t_eReturnCode PCSIM_GetEncoderPosition(t_eFMKIO_InEcdrSignals signal, t_float32 *absolute, t_float32 *relative);
t_eReturnCode PCSIM_SetEncoderSpeed(t_eFMKIO_InEcdrSignals signal, t_float32 speed);
t_eReturnCode PCSIM_GetEncoderSpeed(t_eFMKIO_InEcdrSignals signal, t_float32 *speed);
t_eReturnCode PCSIM_SetPwmDuty(t_eFMKIO_OutPwmSig signal, t_uint16 duty);
t_eReturnCode PCSIM_GetPwmDuty(t_eFMKIO_OutPwmSig signal, t_uint16 *duty);
t_eReturnCode PCSIM_SetPwmFrequency(t_eFMKIO_OutPwmSig signal, t_float32 frequency);
t_eReturnCode PCSIM_GetPwmFrequency(t_eFMKIO_OutPwmSig signal, t_float32 *frequency);
t_eReturnCode PCSIM_SetPwmPulses(t_eFMKIO_OutPwmSig signal, t_uint16 pulses);
t_eReturnCode PCSIM_GetPwmPulses(t_eFMKIO_OutPwmSig signal, t_uint16 *pulses);
t_eReturnCode PCSIM_InjectCanFrame(t_eFMKFDCAN_NodeList node,
                                   t_uint32 identifier,
                                   t_bool isExtended,
                                   const t_uint8 *data,
                                   t_uint8 dlc);
t_uint16 PCSIM_GetCanTxCount(void);
t_eReturnCode PCSIM_PopCanTxFrame(t_sPCSIM_CanTxFrame *frame);
t_eReturnCode PCSIM_ClearCanTxFrames(void);

/* Snapshot helpers */
const t_float32 *PCSIM_GetAnalogSnapshot(void);
const t_uint16 *PCSIM_GetPwmSnapshot(void);

t_uint32 PCSIM_GetTickMs(void);

#ifdef __cplusplus
}
#endif

#endif
