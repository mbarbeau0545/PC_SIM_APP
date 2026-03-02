#include "pc_sim_runtime.h"

#include <stdarg.h>
#include <stdio.h>

static t_cbFMKSRL_RcvMsgEvent *g_pcSimRxCb_apcb[FMKSRL_SERIAL_LINE_NB];
static t_cbFMKSRL_TransmitMsgEvent *g_pcSimTxCb_apcb[FMKSRL_SERIAL_LINE_NB];

static void s_printTxFrame(t_eFMKSRL_SerialLine line_e, t_uint8 *data_pu8, t_uint16 size_u16)
{
    t_uint16 idx_u16;
    printf("[FMKSRL TX L%u] ", (unsigned)line_e);
    for (idx_u16 = 0U; idx_u16 < size_u16; idx_u16++)
    {
        printf("%02X", data_pu8[idx_u16]);
        if (idx_u16 + 1U < size_u16)
        {
            putchar(' ');
        }
    }
    if (size_u16 == 0U)
    {
        printf("<empty>");
    }
    printf("\n");
    fflush(stdout);
}

t_eReturnCode FMKSRL_Init(void) { return RC_OK; }
t_eReturnCode FMKSRL_Cyclic(void) { return RC_OK; }

t_eReturnCode FMKSRL_GetState(t_eCyclicModState *f_State_pe)
{
    if (f_State_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_State_pe = g_pcSimSrlState_e;
    return RC_OK;
}

t_eReturnCode FMKSRL_SetState(t_eCyclicModState f_State_e)
{
    g_pcSimSrlState_e = f_State_e;
    return RC_OK;
}

t_eReturnCode FMKSRL_InitDrv(t_eFMKSRL_SerialLine f_SrlLine_e,
                             t_sFMKSRL_DrvSerialCfg f_SerialCfg_s,
                             t_cbFMKSRL_RcvMsgEvent *f_rcvMsgEvnt_pcb,
                             t_cbFMKSRL_TransmitMsgEvent *f_txMsgEvnt_pcb)
{
    (void)f_SerialCfg_s;
    if (f_SrlLine_e >= FMKSRL_SERIAL_LINE_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }

    g_pcSimRxCb_apcb[f_SrlLine_e] = f_rcvMsgEvnt_pcb;
    g_pcSimTxCb_apcb[f_SrlLine_e] = f_txMsgEvnt_pcb;
    return RC_OK;
}

t_eReturnCode FMKSRL_Transmit(t_eFMKSRL_SerialLine f_SrlLine_e,
                              t_eFMKSRL_TxOpeMode f_OpeMode_e,
                              t_uint8 *f_msgData_pu8,
                              t_uint16 f_dataSize_u16,
                              t_uint16 f_InfoMode_u16,
                              t_bool f_EnableTxCb_b)
{
    (void)f_OpeMode_e;
    (void)f_InfoMode_u16;

    if (f_SrlLine_e >= FMKSRL_SERIAL_LINE_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    if ((f_msgData_pu8 == NULL) && (f_dataSize_u16 > 0U))
    {
        return RC_ERROR_PTR_NULL;
    }

    s_printTxFrame(f_SrlLine_e, f_msgData_pu8, f_dataSize_u16);

    if ((f_EnableTxCb_b == TRUE) && (g_pcSimTxCb_apcb[f_SrlLine_e] != NULL_FUNCTION))
    {
        g_pcSimTxCb_apcb[f_SrlLine_e](TRUE, FMKSRL_CB_INFO_TRANSMIT_OK);
    }

    return RC_OK;
}

t_eReturnCode FMKSRL_ConfigureReception(t_eFMKSRL_SerialLine f_SrlLine_e,
                                        t_eFMKSRL_RxOpeMode f_OpeMode_e,
                                        t_uint16 f_InfoOpe_u16)
{
    (void)f_OpeMode_e;
    (void)f_InfoOpe_u16;
    if (f_SrlLine_e >= FMKSRL_SERIAL_LINE_NB)
    {
        return RC_ERROR_PARAM_INVALID;
    }
    return RC_OK;
}

void FMKSRL_LogUartSend(t_eFMKSRL_SerialLine f_SrlLine_e, const t_char *fmt, ...)
{
    va_list args;
    if (fmt == NULL)
    {
        return;
    }

    printf("[FMKSRL LOG L%u] ", (unsigned)f_SrlLine_e);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

void FMKSRL_PRIVATE_GetHandleTypeDef(t_eFMKSRL_SerialLine f_SrlLine_u8,
                                     UART_HandleTypeDef **f_huartHandle_ps,
                                     USART_HandleTypeDef **f_UsartHandle_ps)
{
    (void)f_SrlLine_u8;
    if (f_huartHandle_ps != NULL)
    {
        *f_huartHandle_ps = (UART_HandleTypeDef *)NULL;
    }
    if (f_UsartHandle_ps != NULL)
    {
        *f_UsartHandle_ps = (USART_HandleTypeDef *)NULL;
    }
}
