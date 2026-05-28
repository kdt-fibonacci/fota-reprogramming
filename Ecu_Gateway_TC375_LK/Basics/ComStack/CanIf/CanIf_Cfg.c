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
        .CanTpTxPduId = CANTP_TXNPDU_GATEWAY_TO_ECU0,
        .CanIfTxPduId = CANIF_TXPDU_GATEWAY_TO_ECU0,
        .CanId        = CAN_ID_ECU0,
        .Hth          = CAN_HTH_LOCAL_TO_REMOTE
    },
    {
        .CanTpTxPduId = CANTP_TXNPDU_GATEWAY_TO_ECU1,
        .CanIfTxPduId = CANIF_TXPDU_GATEWAY_TO_ECU1,
        .CanId        = CAN_ID_ECU1,
        .Hth          = CAN_HTH_LOCAL_TO_REMOTE
    }
};

/*********************************************************************************************************************/
/*--------------------------------------------------Rx L-PDU Configs------------------------------------------------*/
/*********************************************************************************************************************/

const CanIf_RxPduConfigType CanIf_RxPduConfig[CANIF_RXPDU_COUNT] =
{
    {
        .CanTpRxPduId = CANTP_RXNPDU_ECU0_TO_GATEWAY,
        .CanIfRxPduId = CANIF_RXPDU_ECU0_TO_GATEWAY,
        .CanId        = CAN_ID_GATEWAY_ECU0,
        .Hrh          = CAN_HRH_ECU0_TO_GATEWAY
    },
    {
        .CanTpRxPduId = CANTP_RXNPDU_ECU1_TO_GATEWAY,
        .CanIfRxPduId = CANIF_RXPDU_ECU1_TO_GATEWAY,
        .CanId        = CAN_ID_GATEWAY_ECU1,
        .Hrh          = CAN_HRH_ECU1_TO_GATEWAY
    }
};
