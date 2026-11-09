/*********************************************************************
 * @file pc_sim_fmk_srl.c
 * @brief Simulated serial startup and deferred owned transmissions.
 *********************************************************************/
#include "pc_sim_runtime.h"
#include "FMK_HAL/FMK_CPU/Src/FMK_CPU.h"
#include "FMK_CFG/FMKCFG_ConfigFiles/FMKSRL_ConfigPrivate.h"
#include "FMK_HAL/FMK_SRL/Src/FMKSRL_QueuePrivate.h"
#include <stdarg.h>
#include <stdio.h>

/// @brief Module-owned simulated TX payloads.
static t_uint8 g_PcSimSrlPayload_aau8[FMKSRL_SERIAL_LINE_NB][1024];
/// @brief Shared production reservation queues.
static t_sFMKSRL_TxQueue g_PcSimSrlQueue_as[FMKSRL_SERIAL_LINE_NB];
/// @brief Configured flags published only by cyclic startup.
static t_bool g_PcSimSrlConfigured_ab[FMKSRL_SERIAL_LINE_NB];
/// @brief Persistent receive callbacks.
static t_cbFMKSRL_RcvMsgEvent * g_PcSimSrlRx_apcb[FMKSRL_SERIAL_LINE_NB];
/// @brief Persistent terminal transmit callbacks.
static t_cbFMKSRL_TransmitMsgEvent * g_PcSimSrlTx_apcb[FMKSRL_SERIAL_LINE_NB];
/// @brief Saved reception modes; configuration never accesses hardware.
static t_eFMKSRL_RxOpeMode g_PcSimSrlRxMode_ae[FMKSRL_SERIAL_LINE_NB];
/// @brief Saved reception sizes or timeouts.
static t_uint16 g_PcSimSrlRxInfo_au16[FMKSRL_SERIAL_LINE_NB];
/// @brief Next startup policy entry to consider.
static t_uint8 g_PcSimSrlStartup_u8;

// Local functions - Prototypes
/** @brief Initialize at most one enabled simulated serial driver per cycle.
 * @retval RC_OK All policy entries processed.
 * @retval RC_WARNING_PENDING Additional entries remain.
 * @retval RC_ERROR_PARAM_INVALID Invalid private settings.
 */
static t_eReturnCode s_PCSIM_ConfigureSrl(void);
/** @brief Send queued copies only on configured lines, in FIFO order.
 * @retval RC_OK Queued messages processed or no pending operation.
 */
static t_eReturnCode s_PCSIM_ServiceSrl(void);
/** @brief Preserve queued requests without configuring hardware.
 * @retval RC_OK No operation required.
 */
static t_eReturnCode s_PCSIM_HoldSrl(void);

/*********************************
 * FMKSRL_Init
 *********************************/
t_eReturnCode FMKSRL_Init(void)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Create software queues without configuring any simulated driver ----//
    g_pcSimSrlState_e = STATE_CYCLIC_CFG;
    g_PcSimSrlStartup_u8 = 0U;
    for(t_uint8 idx_u8 = 0U; (idx_u8 < FMKSRL_SERIAL_LINE_NB) && (Ret_e == RC_OK); idx_u8++)
    {
        g_PcSimSrlConfigured_ab[idx_u8] = FALSE;
        g_PcSimSrlRx_apcb[idx_u8] = NULL_FUNCTION;
        g_PcSimSrlTx_apcb[idx_u8] = NULL_FUNCTION;
        g_PcSimSrlRxMode_ae[idx_u8] = FMKSRL_OPE_RX_NB;
        Ret_e = FMKSRL_QueueInit_PRIVATE_(&g_PcSimSrlQueue_as[idx_u8], g_PcSimSrlPayload_aau8[idx_u8],
                                         sizeof(g_PcSimSrlPayload_aau8[idx_u8]));
    }

    return Ret_e;
}

/*********************************
 * FMKSRL_Cyclic
 *********************************/
t_eReturnCode FMKSRL_Cyclic(void)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Mirror the production lifecycle and keep driver initialization private ----//
    switch(g_pcSimSrlState_e)
    {
        case STATE_CYCLIC_CFG:
        {
            Ret_e = s_PCSIM_ConfigureSrl();
            if(Ret_e == RC_OK)
            {
                g_pcSimSrlState_e = STATE_CYCLIC_PREOPE;
            }
            else if(Ret_e < RC_OK)
            {
                g_pcSimSrlState_e = STATE_CYCLIC_ERROR;
            }
            break;
        }
        case STATE_CYCLIC_PREOPE:
        {
            Ret_e = s_PCSIM_HoldSrl();
            if(Ret_e == RC_OK)
            {
                g_pcSimSrlState_e = STATE_CYCLIC_OPE;
            }
            break;
        }
        case STATE_CYCLIC_OPE:
        {
            Ret_e = s_PCSIM_ServiceSrl();
            if(Ret_e < RC_OK)
            {
                g_pcSimSrlState_e = STATE_CYCLIC_ERROR;
            }
            break;
        }
        default:
        {
            Ret_e = s_PCSIM_HoldSrl();
            break;
        }
    }

    return Ret_e;
}

/*********************************
 * FMKSRL_GetState
 *********************************/
t_eReturnCode FMKSRL_GetState(t_eCyclicModState * f_State_pe)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Return the public lifecycle state ----//
    if(f_State_pe == NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        *f_State_pe = g_pcSimSrlState_e;
    }

    return Ret_e;
}

/*********************************
 * FMKSRL_SetState
 *********************************/
t_eReturnCode FMKSRL_SetState(t_eCyclicModState f_State_e)
{
    //---- 1- Apply the requested lifecycle state ----//
    g_pcSimSrlState_e = f_State_e;

    return RC_OK;
}

/*********************************
 * FMKSRL_ConfigureCallbacks
 *********************************/
t_eReturnCode FMKSRL_ConfigureCallbacks(t_eFMKSRL_SerialLine f_SrlLine_e,
                                       t_cbFMKSRL_RcvMsgEvent * f_Rx_pcb,
                                       t_cbFMKSRL_TransmitMsgEvent * f_Tx_pcb)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Register callbacks without requiring an initialized driver ----//
    if((t_uint32)f_SrlLine_e >= FMKSRL_SERIAL_LINE_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else
    {
        g_PcSimSrlRx_apcb[f_SrlLine_e] = f_Rx_pcb;
        g_PcSimSrlTx_apcb[f_SrlLine_e] = f_Tx_pcb;
    }

    return Ret_e;
}

/*********************************
 * FMKSRL_Transmit
 *********************************/
t_eReturnCode FMKSRL_Transmit(t_eFMKSRL_SerialLine f_SrlLine_e, t_eFMKSRL_TxOpeMode f_OpeMode_e,
                              const t_uint8 * f_msgData_pu8, t_uint16 f_dataSize_u16,
                              t_uint16 f_InfoMode_u16, t_bool f_EnableTxCb_b)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Enqueue an owned copy independently of simulated hardware readiness ----//
    if(((t_uint32)f_SrlLine_e >= FMKSRL_SERIAL_LINE_NB) || ((t_uint32)f_OpeMode_e >= FMKSRL_TX_NB))
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else
    {
        t_sFMKSRL_TxTask task_s = { .dataSize_u16 = f_dataSize_u16, .mode_e = f_OpeMode_e,
                                   .info_u16 = f_InfoMode_u16, .notify_b = f_EnableTxCb_b };
        Ret_e = FMKSRL_QueuePush_PRIVATE_(&g_PcSimSrlQueue_as[f_SrlLine_e], task_s, f_msgData_pu8);
    }

    return Ret_e;
}

/*********************************
 * FMKSRL_ConfigureReception
 *********************************/
t_eReturnCode FMKSRL_ConfigureReception(t_eFMKSRL_SerialLine f_SrlLine_e,
                                        t_eFMKSRL_RxOpeMode f_OpeMode_e, t_uint16 f_InfoOpe_u16)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Preserve software receive configuration before cyclic startup ----//
    if(((t_uint32)f_SrlLine_e >= FMKSRL_SERIAL_LINE_NB) || ((t_uint32)f_OpeMode_e >= FMKSRL_OPE_RX_NB))
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else
    {
        g_PcSimSrlRxMode_ae[f_SrlLine_e] = f_OpeMode_e;
        g_PcSimSrlRxInfo_au16[f_SrlLine_e] = f_InfoOpe_u16;
    }

    return Ret_e;
}

/*********************************
 * FMKSRL_LogUartSend
 *********************************/
t_eReturnCode FMKSRL_LogUartSend(t_eFMKSRL_SerialLine f_SrlLine_e, const t_char * f_Format_pc, ...)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Validate the line and format before building a bounded local message ----//
    if((t_uint32)f_SrlLine_e >= FMKSRL_SERIAL_LINE_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if(f_Format_pc == NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        t_char message_ac[256];
        t_uint32 tick_u32 = 0U;
        FMKCPU_GetTick(&tick_u32);
        int prefix_s32 = snprintf(message_ac, sizeof(message_ac), "[%lu] ", (unsigned long)tick_u32);
        if((prefix_s32 < 0) || ((size_t)prefix_s32 >= sizeof(message_ac)))
        {
            Ret_e = RC_ERROR_WRONG_RESULT;
        }
        else
        {
            //---- 2- Clamp formatted output to actual storage before copying it into the TX queue ----//
            va_list args_s;
            va_start(args_s, f_Format_pc);
            int length_s32 = vsnprintf(&message_ac[prefix_s32], sizeof(message_ac) - prefix_s32,
                                       f_Format_pc, args_s);
            va_end(args_s);
            if(length_s32 < 0)
            {
                Ret_e = RC_ERROR_WRONG_RESULT;
            }
            else
            {
                size_t size_u32 = (size_t)prefix_s32 + (size_t)length_s32;
                if(size_u32 >= sizeof(message_ac))
                {
                    size_u32 = sizeof(message_ac) - 1U;
                }
                Ret_e = FMKSRL_Transmit(f_SrlLine_e, FMKSRL_TX_ONESHOT, (const t_uint8 *)message_ac,
                                        (t_uint16)size_u32, 0U, FALSE);
            }
        }
    }

    return Ret_e;
}

/*********************************
 * FMKSRL_PRIVATE_GetHandleTypeDef
 *********************************/
void FMKSRL_PRIVATE_GetHandleTypeDef(t_eFMKSRL_SerialLine f_SrlLine_u8,
                                     UART_HandleTypeDef ** f_huartHandle_ps,
                                     USART_HandleTypeDef ** f_UsartHandle_ps)
{
    //---- 1- Native drivers do not expose hardware handles ----//
    (void)f_SrlLine_u8;
    if(f_huartHandle_ps != NULL)
    {
        *f_huartHandle_ps = NULL;
    }
    if(f_UsartHandle_ps != NULL)
    {
        *f_UsartHandle_ps = NULL;
    }

    return;
}

/*********************************
 * s_PCSIM_ConfigureSrl
 *********************************/
static t_eReturnCode s_PCSIM_ConfigureSrl(void)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Apply one private policy entry and preserve previously queued application requests ----//
    if(g_PcSimSrlStartup_u8 < FMKSRL_SERIAL_LINE_NB)
    {
        const t_sFMKSRL_LineStartupCfg * cfg_ps = &c_FmkSrl_LineStartupCfg_as[g_PcSimSrlStartup_u8];
        if(cfg_ps->initialize_b == TRUE)
        {
            if((cfg_ps->driverCfg_s.runMode_e >= FMKSRL_LINE_RUNMODE_NB)
            || (cfg_ps->driverCfg_s.hwProtType_e >= FMKSRL_HW_PROTOCOL_NB))
            {
                Ret_e = RC_ERROR_PARAM_INVALID;
            }
            else
            {
                g_PcSimSrlConfigured_ab[g_PcSimSrlStartup_u8] = TRUE;
            }
        }
        if(Ret_e == RC_OK)
        {
            g_PcSimSrlStartup_u8++;
            Ret_e = RC_WARNING_PENDING;
        }
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_ServiceSrl
 *********************************/
static t_eReturnCode s_PCSIM_ServiceSrl(void)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Consume one task per configured line using its owned bytes ----//
    for(t_uint8 idx_u8 = 0U; idx_u8 < FMKSRL_SERIAL_LINE_NB; idx_u8++)
    {
        if(g_PcSimSrlConfigured_ab[idx_u8] == TRUE)
        {
            t_sFMKSRL_TxTask task_s = {0};
            t_sFMKSRL_TxQueue * queue_ps = &g_PcSimSrlQueue_as[idx_u8];
            Ret_e = LIBFIFO_Peek(&queue_ps->fifo_s, &task_s, sizeof(task_s));
            if(Ret_e == RC_OK)
            {
                printf("[L%u, %lu] ", (unsigned)idx_u8, (unsigned long)PCSIM_GetTickMs());
                for(t_uint16 byte_u16 = 0U; byte_u16 < task_s.dataSize_u16; byte_u16++)
                {
                    printf("%02X ", queue_ps->buffer_pu8[task_s.startBuffer_u16 + byte_u16]);
                }
                putchar('\n');
                fflush(stdout);
                Ret_e = FMKSRL_QueueRelease_PRIVATE_(queue_ps);
                if((task_s.notify_b == TRUE) && (g_PcSimSrlTx_apcb[idx_u8] != NULL_FUNCTION))
                {
                    g_PcSimSrlTx_apcb[idx_u8](TRUE, FMKSRL_CB_INFO_TRANSMIT_OK);
                }
            }
            else if(Ret_e == RC_WARNING_NO_OPERATION)
            {
                Ret_e = RC_OK;
            }
        }
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_HoldSrl
 *********************************/
static t_eReturnCode s_PCSIM_HoldSrl(void)
{
    //---- 1- Preserve queued requests while the lifecycle is inactive ----//
    t_eReturnCode Ret_e = RC_OK;

    return Ret_e;
}
