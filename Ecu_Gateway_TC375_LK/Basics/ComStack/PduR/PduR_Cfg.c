/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "PduR_Cfg.h"

#include "DoIP_Cfg.h"
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
     * Tester -> DoIP -> PduR -> CanTp -> Target ECU
     *
     * DoIP는 Diagnostic Message의 TargetAddress를 보고
     * DOIP_RXPDU_DIAG_REQ_TO_CANTP를 선택한다.
     *
     * PduR은 DoIPRxPduId를 기준으로 CanTpTxNsduId로 변환한다.
     */
    {
        .PduRRoutingPathId = PDUR_ROUTE_DOIP_TO_CANTP_TESTER_TO_TARGET,

        .SourceModule      = PDUR_MODULE_DOIPTP,
        .SourcePduId       = DOIP_RXPDU_DIAG_REQ_TO_CANTP,

        .DestModule        = PDUR_MODULE_CANTP,
        .DestPduId         = CANTP_TXNSDU_LOCAL_TO_REMOTE,

        .RoutingEvent      = PDUR_EVENT_RX_INDICATION
    },

    /*
     * Route 1
     *
     * Target ECU -> CanTp -> PduR -> DoIP -> Tester
     *
     * CanTp는 Target ECU의 UDS Response를 재조립한 뒤
     * PduR-facing RxPduId 기준으로 PduR에 전달한다.
     *
     * PduR은 CanTp가 보고한 PduR-facing RxPduId를 DoIPTxPduId로 변환한다.
     */
    {
        .PduRRoutingPathId = PDUR_ROUTE_CANTP_TO_DOIP_TARGET_TO_TESTER,

        .SourceModule      = PDUR_MODULE_CANTP,
        .SourcePduId       = PDUR_RXPDU_CANTP_TARGET_TO_TESTER,

        .DestModule        = PDUR_MODULE_DOIPTP,
        .DestPduId         = DOIP_TXPDU_DIAG_RES_FROM_CANTP,

        .RoutingEvent      = PDUR_EVENT_RX_INDICATION
    }
};
