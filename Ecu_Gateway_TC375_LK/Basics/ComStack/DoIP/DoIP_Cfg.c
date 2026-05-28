/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "DoIP_Cfg.h"
#include "SoAd_Cfg.h"

/*********************************************************************************************************************/
/*------------------------------------------------Global Constants---------------------------------------------------*/
/*********************************************************************************************************************/

const DoIP_ConfigType DoIP_Config =
{
    .EntityLogicalAddress        = DOIP_LOGICAL_ADDRESS_ECU,
    .TesterLogicalAddress        = DOIP_LOGICAL_ADDRESS_TESTER,

    .SoAdRxPduId                 = SOAD_RXPDU_DOIP_TCP,
    .SoAdTxPduId                 = SOAD_TXPDU_DOIP_TCP
};

/*
 * Rx 방향 매핑
 *
 * DoIP Diagnostic Message의 TargetAddress를 보고
 * PduR에 넘길 DoIPRxPduId를 결정한다.
 *
 * 현재 프로젝트에서는 ECU0/ECU1 방향 라우팅만 수행한다.
 *
 * TargetAddress = DOIP_LOGICAL_ADDRESS_TARGET_ECU_0
 * -> DoIPRxPduId = DOIP_RXPDU_DIAG_REQ_TO_CANTP_ECU0
 * -> PduR route  = DoIP -> CanTp ECU0
 */
const DoIP_RxPduConfigType DoIP_RxPduConfig[DOIP_RXPDU_COUNT] =
{
    {
        .DoIPRxPduId   = DOIP_RXPDU_DIAG_REQ_TO_CANTP_ECU0,
        .TargetAddress = DOIP_LOGICAL_ADDRESS_TARGET_ECU_0
    },
    {
        .DoIPRxPduId   = DOIP_RXPDU_DIAG_REQ_TO_CANTP_ECU1,
        .TargetAddress = DOIP_LOGICAL_ADDRESS_TARGET_ECU_1
    }
};

/*
 * Tx 방향 매핑
 *
 * PduR이 DoIP_TpTransmit() 호출 시 넘긴 DoIPTxPduId를 보고
 * DoIP Diagnostic Message의 SourceAddress를 결정한다.
 *
 * 현재 프로젝트에서는 CAN ECU0/ECU1의 응답을 Tester에게 전달한다.
 *
 * DoIPTxPduId = DOIP_TXPDU_DIAG_RES_FROM_CANTP_ECU0
 * -> SourceAddress = DOIP_LOGICAL_ADDRESS_TARGET_ECU_0
 */
const DoIP_TxPduConfigType DoIP_TxPduConfig[DOIP_TXPDU_COUNT] =
{
    {
        .DoIPTxPduId   = DOIP_TXPDU_DIAG_RES_FROM_CANTP_ECU0,
        .SourceAddress = DOIP_LOGICAL_ADDRESS_TARGET_ECU_0
    },
    {
        .DoIPTxPduId   = DOIP_TXPDU_DIAG_RES_FROM_CANTP_ECU1,
        .SourceAddress = DOIP_LOGICAL_ADDRESS_TARGET_ECU_1
    }
};
