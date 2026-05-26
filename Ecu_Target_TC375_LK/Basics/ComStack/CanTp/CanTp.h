#ifndef CANTP_H_
#define CANTP_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "Std_Types.h"
#include "ComStack_Types.h"

/*********************************************************************************************************************/
/*------------------------------------------------------APIs---------------------------------------------------------*/
/*********************************************************************************************************************/

void CanTp_Init(void);

/*
 * CanTpTxSduId:
 *   상위 계층(PduR)이 CanTp에게 전송을 요청하는 Tx N-SDU handle.
 *   하나의 UDS payload 전체를 식별한다.
 *
 * CanTpTxInfoPtr:
 *   전송할 N-SDU payload pointer와 전체 length를 담는 ComStack 표준 타입.
 */
Std_ReturnType CanTp_Transmit(
    PduIdType CanTpTxSduId,
    const PduInfoType* CanTpTxInfoPtr
);

/*
 * CanTpRxNPduId:
 *   CanIf가 CanTp에게 전달하는 Rx N-PDU handle.
 *   CAN frame 하나가 어떤 CanTp connection에 속하는지 식별한다.
 *
 * PduInfoPtr:
 *   수신된 CAN N-PDU payload pointer와 length.
 */
void CanTp_RxIndication(
    PduIdType CanTpRxNPduId,
    const PduInfoType* PduInfoPtr
);

/*
 * CanTpTxNPduId:
 *   CanIf가 CanTp에게 전달하는 Tx N-PDU confirmation handle.
 *   CanTp가 CanIf_Transmit()으로 요청한 SF/FF/CF/FC frame 하나의 송신 완료를 의미한다.
 *
 * result:
 *   하위 N-PDU 송신 결과.
 */
void CanTp_TxConfirmation(
    PduIdType CanTpTxNPduId,
    Std_ReturnType result
);

/*
 * 베어메탈 polling 기반 CanTp 상태머신.
 * blocking wait 없이 TX/RX 상태를 진행시킨다.
 */
void CanTp_MainFunction(void);

#endif /* CANTP_H_ */