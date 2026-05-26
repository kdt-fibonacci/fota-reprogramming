/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "PduR_Cfg.h"

#include "Dcm_Cfg.h"
#include "CanTp_Cfg.h"

/*********************************************************************************************************************/
/*------------------------------------------------Global Constants---------------------------------------------------*/
/*********************************************************************************************************************/

const PduR_RoutingPathConfigType
    PduR_RoutingPathConfig[PDUR_ROUTING_PATH_COUNT] =
{
    /*
     * Route 0
     *
     * Gateway → CanTp → PduR → Dcm
     *
     * CanTp가 Gateway에서 온 UDS Request를 모두 수신하면
     * PduR_CanTpRxIndication()을 호출한다.
     *
     * PduR은 CanTp가 보고한 PduR-facing RxPduId를 DcmRxPduId로 변환하여
     * Dcm_RxIndication()을 호출한다.
     */
    {
        .PduRRoutingPathId = PDUR_ROUTE_CANTP_TO_DCM_REMOTE_TO_LOCAL,

        .SourceModule      = PDUR_MODULE_CANTP,
        .SourcePduId       = PDUR_RXPDU_CANTP_REMOTE_TO_LOCAL,

        .DestModule        = PDUR_MODULE_DCM,
        .DestPduId         = DCM_RXPDU_DIAG_REQ,

        .RoutingEvent      = PDUR_EVENT_RX_INDICATION
    },

    /*
     * Route 1
     *
     * Dcm → PduR → CanTp → Gateway
     *
     * Dcm이 UDS Response 전송을 요청하면
     * PduR_DcmTransmit()을 호출한다.
     *
     * PduR은 DcmTxPduId를 CanTpTxNsduId로 변환하여
     * CanTp_Transmit()을 호출한다.
     */
    {
        .PduRRoutingPathId = PDUR_ROUTE_DCM_TO_CANTP_LOCAL_TO_REMOTE,

        .SourceModule      = PDUR_MODULE_DCM,
        .SourcePduId       = DCM_TXPDU_DIAG_RES,

        .DestModule        = PDUR_MODULE_CANTP,
        .DestPduId         = CANTP_TXNSDU_LOCAL_TO_REMOTE,

        .RoutingEvent      = PDUR_EVENT_TRANSMIT
    },

    /*
     * Route 2
     *
     * CanTp → PduR → Dcm
     *
     * CanTp가 UDS Response 송신 완료/실패를 알리면
     * PduR_CanTpTxConfirmation()을 호출한다.
     *
     * PduR은 CanTp가 보고한 PduR-facing TxConfirmation PDU ID를 DcmTxPduId로 변환하여
     * Dcm_TxConfirmation()을 호출한다.
     */
    {
        .PduRRoutingPathId = PDUR_ROUTE_CANTP_TXCONF_TO_DCM_LOCAL_TO_REMOTE,

        .SourceModule      = PDUR_MODULE_CANTP,
        .SourcePduId       = PDUR_TXCONF_CANTP_LOCAL_TO_REMOTE,

        .DestModule        = PDUR_MODULE_DCM,
        .DestPduId         = DCM_TXPDU_DIAG_RES,

        .RoutingEvent      = PDUR_EVENT_TX_CONFIRMATION
    }
};
