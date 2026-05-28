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
        .CanTpTxNsduId   = CANTP_TXNSDU_LOCAL_TO_REMOTE,
        .CanIfTxNpduId   = CANIF_TXPDU_LOCAL_TO_REMOTE,
        .PduRTxPduId     = PDUR_TXCONF_CANTP_LOCAL_TO_REMOTE,
        .ExpectedRxNsduId = CANTP_RXNSDU_REMOTE_TO_LOCAL
    }
};

/*********************************************************************************************************************/
/*--------------------------------------------------Rx N-SDU Configs------------------------------------------------*/
/*********************************************************************************************************************/

const CanTp_RxNsduConfigType CanTp_RxNsduConfig[CANTP_RXNSDU_COUNT] =
{
    {
        .CanTpRxNsduId  = CANTP_RXNSDU_REMOTE_TO_LOCAL,
        .CanIfRxNpduId  = CANIF_RXPDU_REMOTE_TO_LOCAL,
        .CanIfTxFcPduId = CANIF_TXPDU_LOCAL_TO_REMOTE,
        .PduRRxPduId    = PDUR_RXPDU_CANTP_REMOTE_TO_LOCAL,
        .RxBufferSize   = CANTP_RX_BUFFER_SIZE,
        .BlockSize      = CANTP_DEFAULT_BLOCK_SIZE,
        .STmin          = CANTP_DEFAULT_STMIN
    }
};
