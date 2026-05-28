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
    .TesterLogicalAddressDefault = DOIP_LOGICAL_ADDRESS_TESTER_DEFAULT,

    .SoAdRxPduId                 = SOAD_RXPDU_DOIP_TCP,
    .SoAdTxPduId                 = SOAD_TXPDU_DOIP_TCP
};

/*
 * Rx 방향 매핑
 *
 * DoIP Diagnostic Message의 TargetAddress를 보고
 * PduR에 넘길 DoIPRxPduId를 결정한다.
 *
 * 현재 프로젝트에서는 Target ECU 방향 라우팅만 수행한다.
 *
 * TargetAddress = DOIP_LOGICAL_ADDRESS_TARGET_ECU
 * → DoIPRxPduId = DOIP_RXPDU_DIAG_REQ_TO_CANTP
 * → PduR route  = DoIP → CanTp
 */
const DoIP_RxPduConfigType DoIP_RxPduConfig[DOIP_RXPDU_COUNT] =
{
    {
        .DoIPRxPduId   = DOIP_RXPDU_DIAG_REQ_TO_CANTP,
        .TargetAddress = DOIP_LOGICAL_ADDRESS_TARGET_ECU
    }
};

/*
 * Tx 방향 매핑
 *
 * PduR이 DoIP_TpTransmit() 호출 시 넘긴 DoIPTxPduId를 보고
 * DoIP Diagnostic Message의 SourceAddress를 결정한다.
 *
 * 현재 프로젝트에서는 CAN Target ECU의 응답만 Tester에게 전달한다.
 *
 * DoIPTxPduId = DOIP_TXPDU_DIAG_RES_FROM_CANTP
 * → SourceAddress = DOIP_LOGICAL_ADDRESS_TARGET_ECU
 */
const DoIP_TxPduConfigType DoIP_TxPduConfig[DOIP_TXPDU_COUNT] =
{
    {
        .DoIPTxPduId   = DOIP_TXPDU_DIAG_RES_FROM_CANTP,
        .SourceAddress = DOIP_LOGICAL_ADDRESS_TARGET_ECU
    }
};