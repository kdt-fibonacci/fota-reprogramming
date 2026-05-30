/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "CanIf.h"
#include "CanIf_Cfg.h"

#include "Can.h"
#include "CanTp.h"

#include "Debug_Log.h"

#include <string.h>

/*********************************************************************************************************************/
/*----------------------------------------------Function Declarations------------------------------------------------*/
/*********************************************************************************************************************/

static const CanIf_TxPduConfigType* CanIf_FindTxPduConfig(
    PduIdType CanIfTxPduId
);

static const CanIf_RxPduConfigType* CanIf_FindRxPduConfig(
    Can_HwHandleType Hoh,
    Can_IdType CanId
);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void CanIf_Init(void)
{
    /*
     * 현재 CanIf는 별도 runtime 상태를 갖지 않는다.
     * 향후 Controller Mode, PDU Mode 등이 필요하면 여기서 초기화한다.
     */
}

Std_ReturnType CanIf_Transmit(
    PduIdType CanIfTxPduId,
    const PduInfoType* PduInfoPtr
)
{
    const CanIf_TxPduConfigType* TxConfig;
    Can_PduType CanPdu;

    if (PduInfoPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (PduInfoPtr->SduDataPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (PduInfoPtr->SduLength > CAN_MAX_DATA_PAYLOAD)
    {
        return E_NOT_OK;
    }

    TxConfig = CanIf_FindTxPduConfig(CanIfTxPduId);

    if (TxConfig == NULL_PTR)
    {
        return E_NOT_OK;
    }

    /*
     * CanIf는 상위 계층에서 받은 SDU를
     * Can Driver가 이해할 수 있는 Can_PduType으로 변환한다.
     *
     * swPduHandle:
     *   송신 완료 시 Can Driver가 CanIf_TxConfirmation()으로 되돌려줄 식별자.
     */
    CanPdu.swPduHandle = CanIfTxPduId;
    CanPdu.id          = TxConfig->CanId;
    CanPdu.length      = (uint8)PduInfoPtr->SduLength;
    CanPdu.sdu         = PduInfoPtr->SduDataPtr;

    CANIF_DEBUG_PRINTF(
        "[CanIf][TX] CanIfTxPduId=%u Hth=%u CanId=0x%03X CanTpTxPduId=%u\r\n",
        CanIfTxPduId,
        TxConfig->Hth,
        TxConfig->CanId,
        TxConfig->CanTpTxPduId
    );

    CANIF_DEBUG_PRINT_PDU(
        "[CanIf][TX] PDU",
        PduInfoPtr
    );

    return Can_Write(
        TxConfig->Hth,
        &CanPdu
    );
}

void CanIf_RxIndication(
    const Can_HwType* Mailbox,
    const PduInfoType* PduInfoPtr
)
{
    const CanIf_RxPduConfigType* RxConfig;

    if (Mailbox == NULL_PTR)
    {
        return;
    }

    if (PduInfoPtr == NULL_PTR)
    {
        return;
    }

    if (PduInfoPtr->SduDataPtr == NULL_PTR)
    {
        return;
    }

    RxConfig = CanIf_FindRxPduConfig(
        Mailbox->Hoh,
        Mailbox->CanId
    );

    if (RxConfig == NULL_PTR)
    {
        return;
    }

    /*
     * 현재 프로젝트에서는 CanIf의 상위 계층을 CanTp 하나로 고정한다.
     * CanIf Rx L-PDU를 CanTp Rx N-PDU로 매핑하여 전달한다.
     */
    CANIF_DEBUG_PRINTF(
        "[CanIf][RX] Hoh=%u CanId=0x%03X CanTpRxPduId=%u\r\n",
        Mailbox->Hoh,
        Mailbox->CanId,
        RxConfig->CanTpRxPduId
    );

    CANIF_DEBUG_PRINT_PDU(
        "[CanIf][RX] PDU",
        PduInfoPtr
    );

    CanTp_RxIndication(
        RxConfig->CanTpRxPduId,
        PduInfoPtr
    );
}

void CanIf_TxConfirmation(
    PduIdType CanIfTxPduId,
    Std_ReturnType result
)
{
    const CanIf_TxPduConfigType* TxConfig;

    TxConfig = CanIf_FindTxPduConfig(CanIfTxPduId);

    if (TxConfig == NULL_PTR)
    {
        return;
    }

    /*
     * Can Driver의 TxConfirmation은 CAN L-PDU 하나의 송신 완료를 의미한다.
     * 현재 구현에서는 Can Driver에서 보낸 결과를 bypass한다.
     */
    CANIF_DEBUG_PRINTF(
        "[CanIf][TX-CNF] CanIfTxPduId=%u CanTpTxPduId=%u Result=%u\r\n",
        CanIfTxPduId,
        TxConfig->CanTpTxPduId,
        result
    );

    CanTp_TxConfirmation(
        TxConfig->CanTpTxPduId,
        result
    );
}

/*********************************************************************************************************************/
/*----------------------------------------------Private Functions-----------------------------------------------------*/
/*********************************************************************************************************************/

static const CanIf_TxPduConfigType* CanIf_FindTxPduConfig(
    PduIdType CanIfTxPduId
)
{
    uint8 Index;

    for (Index = 0U; Index < CANIF_TXPDU_COUNT; Index++)
    {
        if (CanIf_TxPduConfig[Index].CanIfTxPduId == CanIfTxPduId)
        {
            return &CanIf_TxPduConfig[Index];
        }
    }

    return NULL_PTR;
}

static const CanIf_RxPduConfigType* CanIf_FindRxPduConfig(
    Can_HwHandleType Hoh,
    Can_IdType CanId
)
{
    uint8 Index;

    for (Index = 0U; Index < CANIF_RXPDU_COUNT; Index++)
    {
        if ((CanIf_RxPduConfig[Index].Hrh == Hoh) &&
            (CanIf_RxPduConfig[Index].CanId == CanId))
        {
            return &CanIf_RxPduConfig[Index];
        }
    }

    return NULL_PTR;
}
