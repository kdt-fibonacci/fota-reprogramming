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
     * Tester -> DoIP -> PduR -> CanTp -> ECU0
     *
     * DoIP는 Diagnostic Message의 TargetAddress를 보고
     * ECU0용 DoIPRxPduId를 선택한다.
     *
     * PduR은 DoIPRxPduId를 기준으로 CanTpTxNsduId로 변환한다.
     */
    {
        .PduRRoutingPathId = PDUR_ROUTE_DOIP_TO_CANTP_TESTER_TO_ECU0,

        .SourceModule      = PDUR_MODULE_DOIPTP,
        .SourcePduId       = DOIP_RXPDU_DIAG_REQ_TO_CANTP_ECU0,

        .DestModule        = PDUR_MODULE_CANTP,
        .DestPduId         = CANTP_TXNSDU_GATEWAY_TO_ECU0,

        .RoutingEvent      = PDUR_EVENT_RX_INDICATION
    },

    /*
     * Route 1
     *
     * Tester -> DoIP -> PduR -> CanTp -> ECU1
     */
    {
        .PduRRoutingPathId = PDUR_ROUTE_DOIP_TO_CANTP_TESTER_TO_ECU1,

        .SourceModule      = PDUR_MODULE_DOIPTP,
        .SourcePduId       = DOIP_RXPDU_DIAG_REQ_TO_CANTP_ECU1,

        .DestModule        = PDUR_MODULE_CANTP,
        .DestPduId         = CANTP_TXNSDU_GATEWAY_TO_ECU1,

        .RoutingEvent      = PDUR_EVENT_RX_INDICATION
    },

    /*
     * Route 2
     *
     * ECU0 -> CanTp -> PduR -> DoIP -> Tester
     *
     * CanTp는 Target ECU의 UDS Response를 재조립한 뒤
     * PduR-facing RxPduId 기준으로 PduR에 전달한다.
     *
     * PduR은 CanTp가 보고한 PduR-facing RxPduId를 DoIPTxPduId로 변환한다.
     */
    {
        .PduRRoutingPathId = PDUR_ROUTE_CANTP_TO_DOIP_ECU0_TO_TESTER,

        .SourceModule      = PDUR_MODULE_CANTP,
        .SourcePduId       = PDUR_RXPDU_CANTP_ECU0_TO_TESTER,

        .DestModule        = PDUR_MODULE_DOIPTP,
        .DestPduId         = DOIP_TXPDU_DIAG_RES_FROM_CANTP_ECU0,

        .RoutingEvent      = PDUR_EVENT_RX_INDICATION
    },

    /*
     * Route 3
     *
     * ECU1 -> CanTp -> PduR -> DoIP -> Tester
     */
    {
        .PduRRoutingPathId = PDUR_ROUTE_CANTP_TO_DOIP_ECU1_TO_TESTER,

        .SourceModule      = PDUR_MODULE_CANTP,
        .SourcePduId       = PDUR_RXPDU_CANTP_ECU1_TO_TESTER,

        .DestModule        = PDUR_MODULE_DOIPTP,
        .DestPduId         = DOIP_TXPDU_DIAG_RES_FROM_CANTP_ECU1,

        .RoutingEvent      = PDUR_EVENT_RX_INDICATION
    }
};
