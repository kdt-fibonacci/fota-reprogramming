/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "CanTp_Cfg.h"

/*********************************************************************************************************************/
/*--------------------------------------------------Tx N-SDU Configs------------------------------------------------*/
/*********************************************************************************************************************/

const CanTp_TxNsduConfigType CanTp_TxNsduConfig[CANTP_TXNSDU_COUNT] =
{
    {
        .CanTpTxNsduId   = CANTP_TXNSDU_GATEWAY_TO_ECU0,
        .CanIfTxNpduId   = CANIF_TXPDU_GATEWAY_TO_ECU0,
        .PduRTxPduId     = PDUR_TXCONF_CANTP_TESTER_TO_ECU0,
        .ExpectedRxNsduId = CANTP_RXNSDU_ECU0_TO_GATEWAY
    },
    {
        .CanTpTxNsduId   = CANTP_TXNSDU_GATEWAY_TO_ECU1,
        .CanIfTxNpduId   = CANIF_TXPDU_GATEWAY_TO_ECU1,
        .PduRTxPduId     = PDUR_TXCONF_CANTP_TESTER_TO_ECU1,
        .ExpectedRxNsduId = CANTP_RXNSDU_ECU1_TO_GATEWAY
    }
};

/*********************************************************************************************************************/
/*--------------------------------------------------Rx N-SDU Configs------------------------------------------------*/
/*********************************************************************************************************************/

const CanTp_RxNsduConfigType CanTp_RxNsduConfig[CANTP_RXNSDU_COUNT] =
{
    {
        .CanTpRxNsduId  = CANTP_RXNSDU_ECU0_TO_GATEWAY,
        .CanIfRxNpduId  = CANTP_RXNPDU_ECU0_TO_GATEWAY,
        .CanIfTxFcPduId = CANIF_TXPDU_GATEWAY_TO_ECU0,
        .PduRRxPduId    = PDUR_RXPDU_CANTP_ECU0_TO_TESTER,
        .RxBufferSize   = CANTP_RX_BUFFER_SIZE,
        .BlockSize      = CANTP_DEFAULT_BLOCK_SIZE,
        .STmin          = CANTP_DEFAULT_STMIN
    },
    {
        .CanTpRxNsduId  = CANTP_RXNSDU_ECU1_TO_GATEWAY,
        .CanIfRxNpduId  = CANTP_RXNPDU_ECU1_TO_GATEWAY,
        .CanIfTxFcPduId = CANIF_TXPDU_GATEWAY_TO_ECU1,
        .PduRRxPduId    = PDUR_RXPDU_CANTP_ECU1_TO_TESTER,
        .RxBufferSize   = CANTP_RX_BUFFER_SIZE,
        .BlockSize      = CANTP_DEFAULT_BLOCK_SIZE,
        .STmin          = CANTP_DEFAULT_STMIN
    }
};
