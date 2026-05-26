#ifndef PDUR_H_
#define PDUR_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "Std_Types.h"
#include "ComStack_Types.h"

/*********************************************************************************************************************/
/*------------------------------------------------------APIs---------------------------------------------------------*/
/*********************************************************************************************************************/

void PduR_Init(void);

/*
 * Dcm이 UDS Response 또는 Request payload 전송을 PduR에 요청할 때 호출한다.
 *
 * DcmTxPduId:
 *   Dcm 기준 Tx I-PDU handle.
 *
 * PduInfoPtr:
 *   송신할 순수 UDS payload pointer와 length.
 */
Std_ReturnType PduR_DcmTransmit(
    PduIdType DcmTxPduId,
    const PduInfoType* PduInfoPtr
);

/*
 * DoIP_Tp가 Diagnostic Message 수신 완료 후 PduR에 UDS payload를 전달한다.
 *
 * DoIPRxPduId:
 *   DoIP 기준 Rx PDU handle.
 *   DoIP 모듈이 Target Logical Address를 기준으로 선택한 PDU ID이다.
 *
 * PduInfoPtr:
 *   DoIP address field를 제거한 순수 UDS payload.
 */
void PduR_DoIPTpRxIndication(
    PduIdType DoIPRxPduId,
    const PduInfoType* PduInfoPtr
);

/*
 * DoIP_Tp가 송신 완료/실패를 PduR에 알릴 때 호출한다.
 */
void PduR_DoIPTpTxConfirmation(
    PduIdType DoIPTxPduId,
    Std_ReturnType Result
);

/*
 * CanTp가 전체 N-SDU 수신 완료 후 PduR에 UDS payload를 전달한다.
 *
 * CanTpRxPduId:
 *   CanTp 기준 Rx N-SDU handle.
 *
 * PduInfoPtr:
 *   수신 완료된 순수 UDS payload.
 */
void PduR_CanTpRxIndication(
    PduIdType CanTpRxPduId,
    const PduInfoType* PduInfoPtr
);

/*
 * CanTp가 전체 N-SDU 송신 완료/실패를 PduR에 알릴 때 호출한다.
 */
void PduR_CanTpTxConfirmation(
    PduIdType CanTpTxPduId,
    Std_ReturnType Result
);

#endif /* PDUR_H_ */