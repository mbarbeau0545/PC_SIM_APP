/** @file pc_sim_fmk_can.c @brief Native CAN simulation with framework-owned node startup. */
#include "pc_sim_runtime.h"
#include "Library/FIFO/Src/LIBFifo.h"
#include "FMK_CFG/FMKCFG_ConfigFiles/FMKFDCAN_ConfigPrivate.h"

/// @brief Configuration progress for simulated nodes.
static t_bool g_pcSimCanNodeConfigured_ab[FMKFDCAN_NODE_NB];
/// @brief Driver settings copied from the same private table as the target module.
static t_sFMKFDCAN_DrvNodeCfg
    g_pcSimCanNodeConfiguration_as[FMKFDCAN_NODE_NB];

/// @brief Next node considered during simulated CFG startup.
static t_uint8 g_pcSimCanStartupNode_u8;

// Local functions - Prototypes
/// @brief Per-node owned simulated TX storage, matching target queue capacity.
static t_sPCSIM_CanTxFrame g_PcSimPendingCan_aas[FMKFDCAN_NODE_NB][120];
/// @brief Per-node bounded descriptor FIFOs.
static t_sLIBFIFO_Core g_PcSimPendingCan_as[FMKFDCAN_NODE_NB];

/** @brief Submit queued copies for configured simulated nodes.
 * @retval RC_OK Pending messages submitted or no pending operation.
 */
static t_eReturnCode s_PCSIM_ServiceCan(void);
/** @brief Apply one node's private configuration to the simulated backend.
 * @param[in] f_Node_e Node. @param[in] f_NodeCfg_ps Immutable driver configuration, not retained.
 * @retval RC_OK Configured. @retval RC_ERROR_PARAM_INVALID Invalid settings.
 */
static t_eReturnCode s_PCSIM_ConfigureCanNode(t_eFMKFDCAN_NodeList f_Node_e,
                                             const t_sFMKFDCAN_DrvNodeCfg * f_NodeCfg_ps);
/** @brief Advance one private startup row. @retval RC_OK Complete. @retval RC_WARNING_PENDING Continue. */
static t_eReturnCode s_PCSIM_ConfigureCan(void);
/** @brief Complete simulated activation. @retval RC_OK Ready. */
static t_eReturnCode s_PCSIM_ActivateCan(void);
/** @brief Hold an operational or suspended simulation state. @retval RC_OK Held. */
static t_eReturnCode s_PCSIM_HoldCan(void);

static t_uint16 s_getLogCount(const t_sPCSIM_CanTxLog *log_ps)
{
    return log_ps->count_u16;
}

static t_eReturnCode s_clearLog(t_sPCSIM_CanTxLog *log_ps)
{
    log_ps->head_u16 = 0U;
    log_ps->tail_u16 = 0U;
    log_ps->count_u16 = 0U;
    return RC_OK;
}

static t_eReturnCode s_popLogFrame(t_sPCSIM_CanTxLog *log_ps, t_sPCSIM_CanTxFrame *frame)
{
    if (frame == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    if (log_ps->count_u16 == 0U)
    {
        return RC_WARNING_NO_OPERATION;
    }

    *frame = log_ps->items_as[log_ps->tail_u16];
    log_ps->tail_u16 = (t_uint16)((log_ps->tail_u16 + 1U) % PCSIM_CAN_TX_LOG_MAX);
    log_ps->count_u16--;
    return RC_OK;
}

static void s_pushLogFrame(t_sPCSIM_CanTxLog *log_ps, const t_sPCSIM_CanTxFrame *frame_ps)
{
    log_ps->items_as[log_ps->head_u16] = *frame_ps;
    log_ps->head_u16 = (t_uint16)((log_ps->head_u16 + 1U) % PCSIM_CAN_TX_LOG_MAX);
    if (log_ps->count_u16 < PCSIM_CAN_TX_LOG_MAX)
    {
        log_ps->count_u16++;
    }
    else
    {
        log_ps->tail_u16 = (t_uint16)((log_ps->tail_u16 + 1U) % PCSIM_CAN_TX_LOG_MAX);
    }
}

t_uint16 PCSIM_GetCanTxCount(void)
{
    return s_getLogCount(&g_pcSimCanTxLog_s);
}

t_eReturnCode PCSIM_ClearCanTxFrames(void)
{
    return s_clearLog(&g_pcSimCanTxLog_s);
}

t_eReturnCode PCSIM_PopCanTxFrame(t_sPCSIM_CanTxFrame *frame)
{
    return s_popLogFrame(&g_pcSimCanTxLog_s, frame);
}

t_uint16 PCSIM_GetCanBrokerTxCount(void)
{
    return s_getLogCount(&g_pcSimCanBrokerTxLog_s);
}

t_eReturnCode PCSIM_ClearCanBrokerTxFrames(void)
{
    return s_clearLog(&g_pcSimCanBrokerTxLog_s);
}

t_eReturnCode PCSIM_PopCanBrokerTxFrame(t_sPCSIM_CanTxFrame *frame)
{
    return s_popLogFrame(&g_pcSimCanBrokerTxLog_s, frame);
}

t_eReturnCode PCSIM_InjectCanFrame(t_eFMKFDCAN_NodeList f_Node_e, t_uint32 f_Identifier_u32,
                                    t_bool f_Extended_b, const t_uint8 * f_Data_pu8, t_uint8 f_Dlc_u8)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Do not deliver saved subscriptions until the simulated node is operational ----//
    if(((t_uint32)f_Node_e >= FMKFDCAN_NODE_NB) || (f_Dlc_u8 > 8U) || (f_Data_pu8 == NULL))
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if((g_pcSimCanState_e != STATE_CYCLIC_OPE) || (g_pcSimCanNodeConfigured_ab[f_Node_e] == FALSE))
    {
        Ret_e = RC_WARNING_PENDING;
    }
    else
    {
        t_sFMKFDCAN_RxItemEvent event_s = {0};
        event_s.ItemId_s.Identifier_u32 = f_Identifier_u32;
        event_s.ItemId_s.IdType_e = f_Extended_b ? FMKFDCAN_IDTYPE_EXTENDED : FMKFDCAN_IDTYPE_STANDARD;
        event_s.ItemId_s.FramePurpose_e = FMKFDCAN_FRAME_PURPOSE_DATA;
        event_s.CanMsg_s.Dlc_e = (t_eFMKFDCAN_DataLength)f_Dlc_u8;
        event_s.CanMsg_s.Direction_e = FMKFDCAN_NODE_DIRECTION_RX;
        event_s.CanMsg_s.data_pu8 = (t_uint8 *)f_Data_pu8;
        event_s.timeStamp_32 = PCSIM_GetTickMs();

        //---- 2- Dispatch matching callbacks using borrowed receive bytes ----//
        for(t_uint16 idx_u16 = 0U; idx_u16 < PCSIM_CAN_RX_REG_MAX; idx_u16++)
        {
            const t_sPCSIM_CanRxReg * registration_ps = &g_pcSimCanRxReg_as[idx_u16];
            if((registration_ps->used_b == TRUE) && (registration_ps->node_e == f_Node_e)
            && (registration_ps->cfg_s.ItemId_s.IdType_e == event_s.ItemId_s.IdType_e)
            && (registration_ps->cfg_s.ItemId_s.FramePurpose_e == FMKFDCAN_FRAME_PURPOSE_DATA)
            && ((f_Identifier_u32 & registration_ps->cfg_s.maskId_u32)
             == (registration_ps->cfg_s.ItemId_s.Identifier_u32 & registration_ps->cfg_s.maskId_u32))
            && (registration_ps->cfg_s.callback_cb != NULL_FUNCTION))
            {
                registration_ps->cfg_s.callback_cb(f_Node_e, event_s, FMKFDCAN_NODE_STATE_OK);
            }
        }
    }

    return Ret_e;
}

/*********************************
 * FMKFDCAN_Init
 *********************************/
t_eReturnCode FMKFDCAN_Init(void)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Arm configuration state without configuring any simulated node ----//
    g_pcSimCanState_e = STATE_CYCLIC_CFG;
    g_pcSimCanStartupNode_u8 = 0U;
    for(t_uint8 idx_u8 = 0U; idx_u8 < FMKFDCAN_NODE_NB; idx_u8++)
    {
        g_pcSimCanNodeConfigured_ab[idx_u8] = FALSE;
        t_sLIBFIFO_Cfg cfg_s = { g_PcSimPendingCan_aas[idx_u8], sizeof(t_sPCSIM_CanTxFrame), 120U };
        Ret_e = LIBFIFO_Create(&g_PcSimPendingCan_as[idx_u8], cfg_s);
    }
    for(t_uint16 idx_u16 = 0U; idx_u16 < PCSIM_CAN_RX_REG_MAX; idx_u16++)
    {
        g_pcSimCanRxReg_as[idx_u16].used_b = FALSE;
    }

    return Ret_e;
}

/*********************************
 * FMKFDCAN_Cyclic
 *********************************/
t_eReturnCode FMKFDCAN_Cyclic(void)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Match the framework lifecycle without exporting a driver-init entry point ----//
    switch(g_pcSimCanState_e)
    {
        case STATE_CYCLIC_CFG:
        {
            Ret_e = s_PCSIM_ConfigureCan();
            if(Ret_e == RC_OK)
            {
                g_pcSimCanState_e = STATE_CYCLIC_PREOPE;
            }
            else if(Ret_e < RC_OK)
            {
                g_pcSimCanState_e = STATE_CYCLIC_ERROR;
            }
            break;
        }
        case STATE_CYCLIC_PREOPE:
        {
            Ret_e = s_PCSIM_ActivateCan();
            if(Ret_e == RC_OK)
            {
                g_pcSimCanState_e = STATE_CYCLIC_OPE;
            }
            break;
        }
        case STATE_CYCLIC_OPE:
        {
            Ret_e = s_PCSIM_ServiceCan();
            break;
        }
        default:
        {
            Ret_e = s_PCSIM_HoldCan();
            break;
        }
    }

    return Ret_e;
}

t_eReturnCode FMKFDCAN_GetState(t_eCyclicModState *f_State_pe)
{
    if (f_State_pe == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }
    *f_State_pe = g_pcSimCanState_e;
    return RC_OK;
}

t_eReturnCode FMKFDCAN_SetState(t_eCyclicModState f_State_e)
{
    g_pcSimCanState_e = f_State_e;
    return RC_OK;
}

/*********************************
 * FMKFDCAN_ConfigureRxItemEvent
 *********************************/
t_eReturnCode FMKFDCAN_ConfigureRxItemEvent(t_eFMKFDCAN_NodeList f_Node_e,
                                           t_sFMKFDCAN_RxItemEventCfg f_RxItemCfg_s)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Accept software subscriptions before cyclic driver configuration ----//
    if((t_uint32)f_Node_e >= (t_uint32)FMKFDCAN_NODE_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if(f_RxItemCfg_s.callback_cb == NULL_FUNCTION)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else
    {
        t_uint16 freeSlot_u16 = PCSIM_CAN_RX_REG_MAX;
        t_bool duplicate_b = FALSE;

        //---- 2- Preserve existing subscriptions and locate a single unused slot ----//
        for(t_uint16 idx_u16 = 0U; idx_u16 < PCSIM_CAN_RX_REG_MAX; idx_u16++)
        {
            const t_sPCSIM_CanRxReg * registration_ps = &g_pcSimCanRxReg_as[idx_u16];
            if(registration_ps->used_b == FALSE)
            {
                if(freeSlot_u16 == PCSIM_CAN_RX_REG_MAX)
                {
                    freeSlot_u16 = idx_u16;
                }
            }
            else if((registration_ps->node_e == f_Node_e)
                 && (registration_ps->cfg_s.ItemId_s.Identifier_u32 == f_RxItemCfg_s.ItemId_s.Identifier_u32)
                 && (registration_ps->cfg_s.ItemId_s.IdType_e == f_RxItemCfg_s.ItemId_s.IdType_e)
                 && (registration_ps->cfg_s.ItemId_s.FramePurpose_e == f_RxItemCfg_s.ItemId_s.FramePurpose_e)
                 && (registration_ps->cfg_s.maskId_u32 == f_RxItemCfg_s.maskId_u32)
                 && (registration_ps->cfg_s.callback_cb == f_RxItemCfg_s.callback_cb))
            {
                duplicate_b = TRUE;
            }
        }
        if(duplicate_b == TRUE)
        {
            Ret_e = RC_WARNING_ALREADY_CONFIGURED;
        }
        else if(freeSlot_u16 == PCSIM_CAN_RX_REG_MAX)
        {
            Ret_e = RC_WARNING_BUSY;
        }
        else
        {
            //---- 3- Publish one completed logical subscription ----//
            g_pcSimCanRxReg_as[freeSlot_u16].used_b = TRUE;
            g_pcSimCanRxReg_as[freeSlot_u16].node_e = f_Node_e;
            g_pcSimCanRxReg_as[freeSlot_u16].cfg_s = f_RxItemCfg_s;
        }
    }

    return Ret_e;
}

t_uint16 PCSIM_GetCanRxRegCount(void)
{
    t_uint16 idx_u16;
    t_uint16 count_u16 = 0U;

    for (idx_u16 = 0U; idx_u16 < PCSIM_CAN_RX_REG_MAX; idx_u16++)
    {
        if (g_pcSimCanRxReg_as[idx_u16].used_b == TRUE)
        {
            count_u16++;
        }
    }
    return count_u16;
}

t_eReturnCode PCSIM_GetCanRxRegAt(t_uint16 index_u16, t_sPCSIM_CanRxRegInfo *info_ps)
{
    t_uint16 idx_u16;
    t_uint16 seen_u16 = 0U;

    if (info_ps == NULL)
    {
        return RC_ERROR_PTR_NULL;
    }

    for (idx_u16 = 0U; idx_u16 < PCSIM_CAN_RX_REG_MAX; idx_u16++)
    {
        if (g_pcSimCanRxReg_as[idx_u16].used_b != TRUE)
        {
            continue;
        }
        if (seen_u16 == index_u16)
        {
            info_ps->node_e = g_pcSimCanRxReg_as[idx_u16].node_e;
            info_ps->identifier_u32 = g_pcSimCanRxReg_as[idx_u16].cfg_s.ItemId_s.Identifier_u32;
            info_ps->mask_u32 = g_pcSimCanRxReg_as[idx_u16].cfg_s.maskId_u32;
            info_ps->idType_e = g_pcSimCanRxReg_as[idx_u16].cfg_s.ItemId_s.IdType_e;
            info_ps->purpose_e = g_pcSimCanRxReg_as[idx_u16].cfg_s.ItemId_s.FramePurpose_e;
            return RC_OK;
        }
        seen_u16++;
    }

    return RC_WARNING_NO_OPERATION;
}

t_eReturnCode FMKFDCAN_SendTxItem(t_eFMKFDCAN_NodeList f_Node_e, t_sFMKFDCAN_TxItem f_TxItemCfg_s)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Copy metadata and payload independently of simulated node readiness ----//
    if(((t_uint32)f_Node_e >= FMKFDCAN_NODE_NB) || ((t_uint32)f_TxItemCfg_s.CanMsg_s.Dlc_e > 8U))
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if((f_TxItemCfg_s.CanMsg_s.data_pu8 == NULL) && (f_TxItemCfg_s.CanMsg_s.Dlc_e != FMKFDCAN_DLC_0))
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        t_sPCSIM_CanTxFrame frame_s = {0};
        frame_s.node_e = f_Node_e;
        frame_s.identifier_u32 = f_TxItemCfg_s.ItemId_s.Identifier_u32;
        frame_s.idType_e = f_TxItemCfg_s.ItemId_s.IdType_e;
        frame_s.purpose_e = f_TxItemCfg_s.ItemId_s.FramePurpose_e;
        frame_s.dlc_e = f_TxItemCfg_s.CanMsg_s.Dlc_e;
        for(t_uint8 idx_u8 = 0U; idx_u8 < (t_uint8)frame_s.dlc_e; idx_u8++)
        {
            frame_s.data_au8[idx_u8] = f_TxItemCfg_s.CanMsg_s.data_pu8[idx_u8];
        }
        Ret_e = LIBFIFO_Push(&g_PcSimPendingCan_as[f_Node_e], &frame_s, sizeof(frame_s));
    }

    return Ret_e;
}

FDCAN_HandleTypeDef *FMKFDCAN_PRIVATE_GetHandleTypeDef(t_eFMKFDCAN_NodeList f_Node_e)
{
    (void)f_Node_e;
    return (FDCAN_HandleTypeDef *)NULL;
}

/*********************************
 * s_PCSIM_ConfigureCan
 *********************************/
static t_eReturnCode s_PCSIM_ConfigureCan(void)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Apply one enabled row and skip disabled nodes without invoking their backend ----//
    if(g_pcSimCanStartupNode_u8 < FMKFDCAN_NODE_NB)
    {
        const t_sFMKFDCAN_NodeStartupCfg * startup_ps = &c_FmkFdcan_NodeStartupCfg_as[g_pcSimCanStartupNode_u8];
        if(startup_ps->initialize_b == TRUE)
        {
            Ret_e = s_PCSIM_ConfigureCanNode((t_eFMKFDCAN_NodeList)g_pcSimCanStartupNode_u8,
                                            &startup_ps->driverCfg_s);
        }
        if(Ret_e == RC_OK)
        {
            g_pcSimCanStartupNode_u8++;
            if(g_pcSimCanStartupNode_u8 < FMKFDCAN_NODE_NB)
            {
                Ret_e = RC_WARNING_PENDING;
            }
        }
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_ActivateCan
 *********************************/
static t_eReturnCode s_PCSIM_ActivateCan(void)
{
    //---- 1- Complete the simulated activation stage after private configuration ----//
    t_eReturnCode Ret_e = RC_OK;

    return Ret_e;
}

/*********************************
 * s_PCSIM_HoldCan
 *********************************/
static t_eReturnCode s_PCSIM_HoldCan(void)
{
    //---- 1- Keep the selected simulation state without hardware operations ----//
    t_eReturnCode Ret_e = RC_OK;

    return Ret_e;
}

/*********************************
 * s_PCSIM_ConfigureCanNode
 *********************************/
static t_eReturnCode s_PCSIM_ConfigureCanNode(t_eFMKFDCAN_NodeList f_Node_e,
                                             const t_sFMKFDCAN_DrvNodeCfg * f_NodeCfg_ps)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Validate the simulated node and private immutable settings ----//
    if(f_NodeCfg_ps == NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else if((t_uint32)f_Node_e >= (t_uint32)FMKFDCAN_NODE_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if(g_pcSimCanNodeConfigured_ab[f_Node_e] == TRUE)
    {
        Ret_e = RC_ERROR_ALREADY_CONFIGURED;
    }
    else if((f_NodeCfg_ps->clockDivider_e == 0U)
    || (f_NodeCfg_ps->clockDivider_e >= FMKFDCAN_CLOCK_KERNEL_DIV_NB)
    || ((f_NodeCfg_ps->clockDivider_e > FMKFDCAN_CLOCK_KERNEL_DIV1)
    && (((t_uint8)f_NodeCfg_ps->clockDivider_e % 2U) != 0U)))
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if(f_NodeCfg_ps->ProtocolUse_e >= FMKFDCAN_PROTOCOL_FDCAN_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if(f_NodeCfg_ps->FrameBaudrate_e >= FMKFDCAN_FRAME_BAUDRATE_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if(f_NodeCfg_ps->DataBaudrate_e >= FMKFDCAN_FRAME_BAUDRATE_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if(f_NodeCfg_ps->QueueType_e >= FMKFDCAN_HWQUEUE_TYPE_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if(f_NodeCfg_ps->FifoMode_e >= FMKFDCAN_FIFO_OPEMODE_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else
    {
        //---- 2- Copy settings only after all validation passes ----//
        g_pcSimCanNodeConfiguration_as[f_Node_e] = *f_NodeCfg_ps;
        g_pcSimCanNodeConfigured_ab[f_Node_e] = TRUE;
    }

    return Ret_e;
}

/*********************************
 * s_PCSIM_ServiceCan
 *********************************/
static t_eReturnCode s_PCSIM_ServiceCan(void)
{
    t_eReturnCode Ret_e = RC_OK;

    //---- 1- Transfer copied frames into the submission logs only after startup ----//
    for(t_uint8 node_u8 = 0U; node_u8 < FMKFDCAN_NODE_NB; node_u8++)
    {
        if(g_pcSimCanNodeConfigured_ab[node_u8] == TRUE)
        {
            for(t_uint8 step_u8 = 0U; step_u8 < 10U; step_u8++)
            {
                t_sPCSIM_CanTxFrame frame_s = {0};
                Ret_e = LIBFIFO_Pop(&g_PcSimPendingCan_as[node_u8], &frame_s, sizeof(frame_s));
                if(Ret_e != RC_OK)
                {
                    Ret_e = RC_OK;
                    break;
                }
                frame_s.timeStamp_u32 = PCSIM_GetTickMs();
                s_pushLogFrame(&g_pcSimCanTxLog_s, &frame_s);
                s_pushLogFrame(&g_pcSimCanBrokerTxLog_s, &frame_s);
            }
        }
    }

    return Ret_e;
}
