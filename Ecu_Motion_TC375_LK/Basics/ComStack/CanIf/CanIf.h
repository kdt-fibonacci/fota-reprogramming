#ifndef CANIF_H_
#define CANIF_H_

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can.h"

void CanIf_Init(void);

/*
 * CanIfTxPduId:
 *   CanIf Tx L-PDU handle.
 *   CanIf 설정에서 CAN ID, HTH, Controller 등을 찾는 데 사용된다.
 *
 * PduInfoPtr:
 *   송신할 SDU pointer와 SDU length를 담는 ComStack 표준 타입.
 */
Std_ReturnType CanIf_Transmit(
    PduIdType CanIfTxPduId,
    const PduInfoType* PduInfoPtr
);

/*
 * Mailbox:
 *   Can Driver가 전달하는 수신 Mailbox 정보.
 *
 *   Mailbox->Hoh:
 *     HRH 역할을 하는 Hardware Object Handle.
 *
 *   Mailbox->CanId:
 *     수신 CAN ID.
 *
 *   Mailbox->ControllerId:
 *     수신 Controller ID.
 *
 * PduInfoPtr:
 *   수신 SDU pointer와 실제 SDU length를 담는 ComStack 표준 타입.
 */
void CanIf_RxIndication(
    const Can_HwType* Mailbox,
    const PduInfoType* PduInfoPtr
);

/*
 * CanIfTxPduId:
 *   Can Driver가 Can_PduType.swPduHandle로 저장했다가
 *   Tx 완료 시 되돌려주는 CanIf Tx PDU handle.
 */
void CanIf_TxConfirmation(
    PduIdType CanIfTxPduId,
    Std_ReturnType result
);

#endif /* CANIF_H_ */