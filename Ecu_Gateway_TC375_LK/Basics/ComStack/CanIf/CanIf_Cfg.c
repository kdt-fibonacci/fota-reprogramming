/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "CanIf_Cfg.h"

/*********************************************************************************************************************/
/*--------------------------------------------------Tx L-PDU Configs------------------------------------------------*/
/*********************************************************************************************************************/

const CanIf_TxPduConfigType CanIf_TxPduConfig[CANIF_TXPDU_COUNT] =
{
    {
        .CanTpTxPduId = CANTP_TXNPDU_GATEWAY_TO_MOTION,
        .CanIfTxPduId = CANIF_TXPDU_GATEWAY_TO_MOTION,
        .CanId        = CAN_ID_MOTION,
        .Hth          = CAN_HTH_LOCAL_TO_REMOTE
    },
    {
        .CanTpTxPduId = CANTP_TXNPDU_GATEWAY_TO_LIGHTING,
        .CanIfTxPduId = CANIF_TXPDU_GATEWAY_TO_LIGHTING,
        .CanId        = CAN_ID_LIGHTING,
        .Hth          = CAN_HTH_LOCAL_TO_REMOTE
    }
};

/*********************************************************************************************************************/
/*--------------------------------------------------Rx L-PDU Configs------------------------------------------------*/
/*********************************************************************************************************************/

const CanIf_RxPduConfigType CanIf_RxPduConfig[CANIF_RXPDU_COUNT] =
{
    {
        .CanTpRxPduId = CANTP_RXNPDU_MOTION_TO_GATEWAY,
        .CanIfRxPduId = CANIF_RXPDU_MOTION_TO_GATEWAY,
        .CanId        = CAN_ID_GATEWAY_MOTION,
        .Hrh          = CAN_HRH_MOTION_TO_GATEWAY
    },
    {
        .CanTpRxPduId = CANTP_RXNPDU_LIGHTING_TO_GATEWAY,
        .CanIfRxPduId = CANIF_RXPDU_LIGHTING_TO_GATEWAY,
        .CanId        = CAN_ID_GATEWAY_LIGHTING,
        .Hrh          = CAN_HRH_LIGHTING_TO_GATEWAY
    }
};
