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
        .CanTpTxNsduId   = CANTP_TXNSDU_GATEWAY_TO_MOTION,
        .CanIfTxNpduId   = CANIF_TXPDU_GATEWAY_TO_MOTION,
        .PduRTxPduId     = PDUR_TXCONF_CANTP_TESTER_TO_MOTION,
        .ExpectedRxNsduId = CANTP_RXNSDU_MOTION_TO_GATEWAY
    },
    {
        .CanTpTxNsduId   = CANTP_TXNSDU_GATEWAY_TO_LIGHTING,
        .CanIfTxNpduId   = CANIF_TXPDU_GATEWAY_TO_LIGHTING,
        .PduRTxPduId     = PDUR_TXCONF_CANTP_TESTER_TO_LIGHTING,
        .ExpectedRxNsduId = CANTP_RXNSDU_LIGHTING_TO_GATEWAY
    }
};

/*********************************************************************************************************************/
/*--------------------------------------------------Rx N-SDU Configs------------------------------------------------*/
/*********************************************************************************************************************/

const CanTp_RxNsduConfigType CanTp_RxNsduConfig[CANTP_RXNSDU_COUNT] =
{
    {
        .CanTpRxNsduId  = CANTP_RXNSDU_MOTION_TO_GATEWAY,
        .CanIfRxNpduId  = CANTP_RXNPDU_MOTION_TO_GATEWAY,
        .CanIfTxFcPduId = CANIF_TXPDU_GATEWAY_TO_MOTION,
        .PduRRxPduId    = PDUR_RXPDU_CANTP_MOTION_TO_TESTER,
        .RxBufferSize   = CANTP_RX_BUFFER_SIZE,
        .BlockSize      = CANTP_DEFAULT_BLOCK_SIZE,
        .STmin          = CANTP_DEFAULT_STMIN
    },
    {
        .CanTpRxNsduId  = CANTP_RXNSDU_LIGHTING_TO_GATEWAY,
        .CanIfRxNpduId  = CANTP_RXNPDU_LIGHTING_TO_GATEWAY,
        .CanIfTxFcPduId = CANIF_TXPDU_GATEWAY_TO_LIGHTING,
        .PduRRxPduId    = PDUR_RXPDU_CANTP_LIGHTING_TO_TESTER,
        .RxBufferSize   = CANTP_RX_BUFFER_SIZE,
        .BlockSize      = CANTP_DEFAULT_BLOCK_SIZE,
        .STmin          = CANTP_DEFAULT_STMIN
    }
};
