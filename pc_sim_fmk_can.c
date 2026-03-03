#include "pc_sim_runtime.h"

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

t_eReturnCode PCSIM_InjectCanFrame(t_eFMKFDCAN_NodeList node,
                                   t_uint32 identifier,
                                   t_bool isExtended,
                                   const t_uint8 *data,
                                   t_uint8 dlc)
{
    t_uint16 idx_u16;
    t_sFMKFDCAN_RxItemEvent rxEvent_s;
    t_eFMKFDCAN_IdentifierType idType_e;

    if ((dlc > 8U) || (data == NULL))
    {
        return RC_ERROR_PARAM_INVALID;
    }

    idType_e = isExtended ? FMKFDCAN_IDTYPE_EXTENDED : FMKFDCAN_IDTYPE_STANDARD;

    rxEvent_s.ItemId_s.Identifier_u32 = identifier;
    rxEvent_s.ItemId_s.IdType_e = idType_e;
    rxEvent_s.ItemId_s.FramePurpose_e = FMKFDCAN_FRAME_PURPOSE_DATA;
    rxEvent_s.CanMsg_s.Dlc_e = (t_eFMKFDCAN_DataLength)dlc;
    rxEvent_s.CanMsg_s.Direction_e = FMKFDCAN_NODE_DIRECTION_RX;
    rxEvent_s.CanMsg_s.data_pu8 = (t_uint8 *)data;
    rxEvent_s.timeStamp_32 = PCSIM_GetTickMs();

    for (idx_u16 = 0U; idx_u16 < PCSIM_CAN_RX_REG_MAX; idx_u16++)
    {
        if ((g_pcSimCanRxReg_as[idx_u16].used_b == TRUE) &&
            (g_pcSimCanRxReg_as[idx_u16].node_e == node) &&
            (g_pcSimCanRxReg_as[idx_u16].cfg_s.ItemId_s.IdType_e == idType_e) &&
            (g_pcSimCanRxReg_as[idx_u16].cfg_s.ItemId_s.FramePurpose_e == FMKFDCAN_FRAME_PURPOSE_DATA) &&
            ((identifier & g_pcSimCanRxReg_as[idx_u16].cfg_s.maskId_u32) ==
             (g_pcSimCanRxReg_as[idx_u16].cfg_s.ItemId_s.Identifier_u32 & g_pcSimCanRxReg_as[idx_u16].cfg_s.maskId_u32)) &&
            (g_pcSimCanRxReg_as[idx_u16].cfg_s.callback_cb != NULL_FUNCTION))
        {
            g_pcSimCanRxReg_as[idx_u16].cfg_s.callback_cb(node, rxEvent_s, FMKFDCAN_NODE_STATE_OK);
        }
    }

    return RC_OK;
}

t_eReturnCode FMKFDCAN_Init(void) { return RC_OK; }
t_eReturnCode FMKFDCAN_Cyclic(void) { return RC_OK; }

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

t_eReturnCode FMKFDCAN_ConfigureRxItemEvent(t_eFMKFDCAN_NodeList f_Node_e,
                                            t_sFMKFDCAN_RxItemEventCfg f_RxItemCfg_s)
{
    t_uint16 idx_u16;

    /* Avoid duplicate registration when upper layers re-run configuration. */
    for (idx_u16 = 0U; idx_u16 < PCSIM_CAN_RX_REG_MAX; idx_u16++)
    {
        if ((g_pcSimCanRxReg_as[idx_u16].used_b == TRUE) &&
            (g_pcSimCanRxReg_as[idx_u16].node_e == f_Node_e) &&
            (g_pcSimCanRxReg_as[idx_u16].cfg_s.ItemId_s.Identifier_u32 == f_RxItemCfg_s.ItemId_s.Identifier_u32) &&
            (g_pcSimCanRxReg_as[idx_u16].cfg_s.ItemId_s.IdType_e == f_RxItemCfg_s.ItemId_s.IdType_e) &&
            (g_pcSimCanRxReg_as[idx_u16].cfg_s.ItemId_s.FramePurpose_e == f_RxItemCfg_s.ItemId_s.FramePurpose_e) &&
            (g_pcSimCanRxReg_as[idx_u16].cfg_s.maskId_u32 == f_RxItemCfg_s.maskId_u32) &&
            (g_pcSimCanRxReg_as[idx_u16].cfg_s.callback_cb == f_RxItemCfg_s.callback_cb))
        {
            return RC_WARNING_ALREADY_CONFIGURED;
        }
    }

    for (idx_u16 = 0U; idx_u16 < PCSIM_CAN_RX_REG_MAX; idx_u16++)
    {
        if (g_pcSimCanRxReg_as[idx_u16].used_b == FALSE)
        {
            g_pcSimCanRxReg_as[idx_u16].used_b = TRUE;
            g_pcSimCanRxReg_as[idx_u16].node_e = f_Node_e;
            g_pcSimCanRxReg_as[idx_u16].cfg_s = f_RxItemCfg_s;
            return RC_OK;
        }
    }
    return RC_ERROR_LIMIT_REACHED;
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
    t_sPCSIM_CanTxFrame frame_s;
    t_uint8 dlc_u8;
    t_uint8 idx_u16;

    frame_s.timeStamp_u32 = PCSIM_GetTickMs();
    frame_s.node_e = f_Node_e;
    frame_s.identifier_u32 = f_TxItemCfg_s.ItemId_s.Identifier_u32;
    frame_s.idType_e = f_TxItemCfg_s.ItemId_s.IdType_e;
    frame_s.purpose_e = f_TxItemCfg_s.ItemId_s.FramePurpose_e;
    frame_s.dlc_e = f_TxItemCfg_s.CanMsg_s.Dlc_e;

    dlc_u8 = (t_uint8)f_TxItemCfg_s.CanMsg_s.Dlc_e;
    if (dlc_u8 > 64U)
    {
        dlc_u8 = 64U;
    }
    for (idx_u16 = 0U; idx_u16 < dlc_u8; idx_u16++)
    {
        frame_s.data_au8[idx_u16] = (f_TxItemCfg_s.CanMsg_s.data_pu8 != NULL)
                                        ? f_TxItemCfg_s.CanMsg_s.data_pu8[idx_u16]
                                        : 0U;
    }
    for (; idx_u16 < 64U; idx_u16++)
    {
        frame_s.data_au8[idx_u16] = 0U;
    }

    s_pushLogFrame(&g_pcSimCanTxLog_s, &frame_s);
    s_pushLogFrame(&g_pcSimCanBrokerTxLog_s, &frame_s);
    return RC_OK;
}

FDCAN_HandleTypeDef *FMKFDCAN_PRIVATE_GetHandleTypeDef(t_eFMKFDCAN_NodeList f_Node_e)
{
    (void)f_Node_e;
    return (FDCAN_HandleTypeDef *)NULL;
}
