/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "CanTp.h"
#include "CanTp_Cfg.h"

#include "CanIf.h"
#include "PduR.h"

#include "Debug_Log.h"

#include <string.h>

/*********************************************************************************************************************/
/*------------------------------------------------------Types--------------------------------------------------------*/
/*********************************************************************************************************************/

typedef enum
{
    CANTP_TX_STATE_IDLE = 0U,
    CANTP_TX_STATE_WAIT_FC,
    CANTP_TX_STATE_SEND_CF,
    CANTP_TX_STATE_WAIT_TX_CONFIRMATION
} CanTp_TxStateType;

typedef enum
{
    CANTP_RX_STATE_IDLE = 0U,
    CANTP_RX_STATE_RECEIVING
} CanTp_RxStateType;

typedef struct
{
    CanTp_TxStateType State;

    PduIdType CanTpTxNsduId;
    PduIdType PduRTxPduId;
    PduIdType CanIfTxPduId;
    PduIdType ExpectedRxNsduId;

    uint16 TotalLength;
    uint16 TransmittedLength;

    uint8 NextSequenceNumber;

    uint8 BlockSize;
    uint8 BlockCounter;
    uint8 STmin;
    uint8 WaitFrameCount;

    uint8 Buffer[CANTP_TX_BUFFER_SIZE];
    uint8 CanFrameBuffer[CANTP_CAN_FRAME_LENGTH];
} CanTp_TxRuntimeType;

typedef struct {
    CanTp_RxStateType State;

    PduIdType PduRRxPduId;
    PduIdType CanTpRxNsduId;
    PduIdType CanIfTxFcPduId;

    uint16 TotalLength;
    uint16 ReceivedLength;

    uint8 ExpectedSequenceNumber;
    uint8 BlockCounter;

    uint8 Buffer[CANTP_RX_BUFFER_SIZE];
    uint8 FcFrameBuffer[CANTP_CAN_FRAME_LENGTH];
} CanTp_RxRuntimeType;

/*********************************************************************************************************************/
/*------------------------------------------------Static Variables---------------------------------------------------*/
/*********************************************************************************************************************/

static CanTp_TxRuntimeType CanTp_TxRuntime[CANTP_TXNSDU_COUNT];
static CanTp_RxRuntimeType CanTp_RxRuntime[CANTP_RXNSDU_COUNT];

/*********************************************************************************************************************/
/*----------------------------------------------Private Functions-----------------------------------------------------*/
/*********************************************************************************************************************/

static const CanTp_TxNsduConfigType* CanTp_FindTxNsduConfig(
    PduIdType CanTpTxNsduId
);

static const CanTp_RxNsduConfigType* CanTp_FindRxNsduConfigByNpduId(
    PduIdType CanTpRxNpduId
);

static CanTp_TxRuntimeType* CanTp_GetTxRuntimeByTxNsduId(
    PduIdType CanTpTxNsduId
);

static CanTp_TxRuntimeType* CanTp_GetTxRuntimeByTxNpduId(
    PduIdType CanTpTxNpduId
);

static CanTp_TxRuntimeType* CanTp_FindTxRuntimeWaitingFc(
    PduIdType ExpectedRxNsduId
);

static CanTp_RxRuntimeType* CanTp_GetRxRuntimeByRxNsduId(
    PduIdType CanTpRxNsduId
);

static Std_ReturnType CanTp_SendSingleFrame(
    const CanTp_TxNsduConfigType* TxConfig,
    CanTp_TxRuntimeType* TxRuntime,
    const PduInfoType* CanTpTxInfoPtr
);

static Std_ReturnType CanTp_SendFirstFrame(
    const CanTp_TxNsduConfigType* TxConfig,
    CanTp_TxRuntimeType* TxRuntime
);

static Std_ReturnType CanTp_SendNextConsecutiveFrame(
    CanTp_TxRuntimeType* TxRuntime
);

static Std_ReturnType CanTp_SendFlowControl(
    const CanTp_RxNsduConfigType* RxConfig,
    CanTp_RxRuntimeType* RxRuntime,
    uint8 FlowStatus
);

static void CanTp_HandleSingleFrame(
    const CanTp_RxNsduConfigType* RxConfig,
    CanTp_RxRuntimeType* RxRuntime,
    const PduInfoType* PduInfoPtr
);

static void CanTp_HandleFirstFrame(
    const CanTp_RxNsduConfigType* RxConfig,
    CanTp_RxRuntimeType* RxRuntime,
    const PduInfoType* PduInfoPtr
);

static void CanTp_HandleConsecutiveFrame(
    const CanTp_RxNsduConfigType* RxConfig,
    CanTp_RxRuntimeType* RxRuntime,
    const PduInfoType* PduInfoPtr
);

static void CanTp_HandleFlowControl(
    const CanTp_RxNsduConfigType* RxConfig,
    const PduInfoType* PduInfoPtr
);

static void CanTp_ResetTxRuntime(
    CanTp_TxRuntimeType* TxRuntime
);

static void CanTp_ResetRxRuntime(
    CanTp_RxRuntimeType* RxRuntime
);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void CanTp_Init(void)
{
    uint8 Index;

    for (Index = 0U; Index < CANTP_TXNSDU_COUNT; Index++)
    {
        CanTp_ResetTxRuntime(&CanTp_TxRuntime[Index]);
    }

    for (Index = 0U; Index < CANTP_RXNSDU_COUNT; Index++)
    {
        CanTp_ResetRxRuntime(&CanTp_RxRuntime[Index]);
    }
}

Std_ReturnType CanTp_Transmit(
    PduIdType CanTpTxSduId,
    const PduInfoType* CanTpTxInfoPtr
)
{
    const CanTp_TxNsduConfigType* TxConfig;
    CanTp_TxRuntimeType* TxRuntime;

    if ((CanTpTxInfoPtr == NULL_PTR) ||
        (CanTpTxInfoPtr->SduDataPtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if ((CanTpTxInfoPtr->SduLength == 0U) ||
        (CanTpTxInfoPtr->SduLength > CANTP_MAX_LENGTH_12BIT))
    {
        return E_NOT_OK;
    }

    TxRuntime = CanTp_GetTxRuntimeByTxNsduId(CanTpTxSduId);

    if (TxRuntime == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (TxRuntime->State != CANTP_TX_STATE_IDLE)
    {
        return E_NOT_OK;
    }

    TxConfig = CanTp_FindTxNsduConfig(CanTpTxSduId);

    if (TxConfig == NULL_PTR)
    {
        return E_NOT_OK;
    }

    CANTP_DEBUG_PRINTF(
        "[CanTp][TX] CanTpTxSduId=%u PduRTxPduId=%u CanIfTxPduId=%u\r\n",
        CanTpTxSduId,
        TxConfig->PduRTxPduId,
        TxConfig->CanIfTxNpduId
    );

    CANTP_DEBUG_PRINT_PDU(
        "[CanTp][TX] N-SDU",
        CanTpTxInfoPtr
    );

    if (CanTpTxInfoPtr->SduLength <= CANTP_SF_MAX_PAYLOAD_LENGTH)
    {
        TxRuntime->CanTpTxNsduId    = TxConfig->CanTpTxNsduId;
        TxRuntime->PduRTxPduId      = TxConfig->PduRTxPduId;
        TxRuntime->CanIfTxPduId     = TxConfig->CanIfTxNpduId;
        TxRuntime->ExpectedRxNsduId = TxConfig->ExpectedRxNsduId;

        return CanTp_SendSingleFrame(
            TxConfig,
            TxRuntime,
            CanTpTxInfoPtr
        );
    }

    if (CanTpTxInfoPtr->SduLength > CANTP_TX_BUFFER_SIZE)
    {
        return E_NOT_OK;
    }

    memcpy(
        TxRuntime->Buffer,
        CanTpTxInfoPtr->SduDataPtr,
        CanTpTxInfoPtr->SduLength
    );

    TxRuntime->CanTpTxNsduId      = TxConfig->CanTpTxNsduId;
    TxRuntime->PduRTxPduId        = TxConfig->PduRTxPduId;
    TxRuntime->CanIfTxPduId       = TxConfig->CanIfTxNpduId;
    TxRuntime->ExpectedRxNsduId   = TxConfig->ExpectedRxNsduId;

    TxRuntime->TotalLength        = CanTpTxInfoPtr->SduLength;
    TxRuntime->TransmittedLength  = CANTP_FF_DATA_LENGTH;

    TxRuntime->NextSequenceNumber = 1U;
    TxRuntime->BlockSize          = 0U;
    TxRuntime->BlockCounter       = 0U;
    TxRuntime->STmin              = 0U;
    TxRuntime->WaitFrameCount     = 0U;

    if (CanTp_SendFirstFrame(TxConfig, TxRuntime) != E_OK)
    {
        CanTp_ResetTxRuntime(TxRuntime);
        return E_NOT_OK;
    }

    TxRuntime->State = CANTP_TX_STATE_WAIT_FC;

    return E_OK;
}

void CanTp_RxIndication(
    PduIdType CanTpRxNpduId,
    const PduInfoType* PduInfoPtr
)
{
    const CanTp_RxNsduConfigType* RxConfig;
    CanTp_RxRuntimeType* RxRuntime;
    uint8 PciType;

    if ((PduInfoPtr == NULL_PTR) ||
        (PduInfoPtr->SduDataPtr == NULL_PTR) ||
        (PduInfoPtr->SduLength == 0U))
    {
        return;
    }

    CANTP_DEBUG_PRINTF(
        "[CanTp][RX] CanTpRxNpduId=%u\r\n",
        CanTpRxNpduId
    );

    CANTP_DEBUG_PRINT_PDU(
        "[CanTp][RX] N-PDU",
        PduInfoPtr
    );

    PciType = PduInfoPtr->SduDataPtr[0] & CANTP_PCI_TYPE_MASK;

    RxConfig = CanTp_FindRxNsduConfigByNpduId(CanTpRxNpduId);

    if (RxConfig == NULL_PTR)
    {
        return;
    }

    if (PciType == CANTP_PCI_TYPE_FC)
    {
        CanTp_HandleFlowControl(RxConfig, PduInfoPtr);
        return;
    }

    RxRuntime = CanTp_GetRxRuntimeByRxNsduId(RxConfig->CanTpRxNsduId);

    if (RxRuntime == NULL_PTR)
    {
        return;
    }

    switch (PciType)
    {
        case CANTP_PCI_TYPE_SF:
        {
            CanTp_HandleSingleFrame(RxConfig, RxRuntime, PduInfoPtr);
            break;
        }

        case CANTP_PCI_TYPE_FF:
        {
            CanTp_HandleFirstFrame(RxConfig, RxRuntime, PduInfoPtr);
            break;
        }

        case CANTP_PCI_TYPE_CF:
        {
            CanTp_HandleConsecutiveFrame(RxConfig, RxRuntime, PduInfoPtr);
            break;
        }

        default:
        {
            break;
        }
    }
}

void CanTp_TxConfirmation(
    PduIdType CanTpTxNPduId,
    Std_ReturnType result
)
{
    CanTp_TxRuntimeType* TxRuntime;

    TxRuntime = CanTp_GetTxRuntimeByTxNpduId(CanTpTxNPduId);

    if (TxRuntime == NULL_PTR)
    {
        return;
    }

    CANTP_DEBUG_PRINTF(
        "[CanTp][TX-CNF] CanTpTxNPduId=%u Result=%u State=%u\r\n",
        CanTpTxNPduId,
        result,
        TxRuntime->State
    );

    if (TxRuntime->State == CANTP_TX_STATE_IDLE)
    {
        return;
    }

    if (result != E_OK)
    {
        PduR_CanTpTxConfirmation(
            TxRuntime->PduRTxPduId,
            E_NOT_OK
        );

        CanTp_ResetTxRuntime(TxRuntime);
        return;
    }

    if (TxRuntime->TotalLength == 0U)
    {
        PduR_CanTpTxConfirmation(
            TxRuntime->PduRTxPduId,
            E_OK
        );

        CanTp_ResetTxRuntime(TxRuntime);
        return;
    }

    if (TxRuntime->State == CANTP_TX_STATE_WAIT_TX_CONFIRMATION)
    {
        if (TxRuntime->TransmittedLength >= TxRuntime->TotalLength)
        {
            PduR_CanTpTxConfirmation(
                TxRuntime->PduRTxPduId,
                E_OK
            );

            CanTp_ResetTxRuntime(TxRuntime);
        }
        else if ((TxRuntime->BlockSize != 0U) &&
                 (TxRuntime->BlockCounter >= TxRuntime->BlockSize))
        {
            TxRuntime->State = CANTP_TX_STATE_WAIT_FC;
        }
        else
        {
            TxRuntime->State = CANTP_TX_STATE_SEND_CF;
        }
    }
}

void CanTp_MainFunction(void)
{
    uint8 Index;

    for (Index = 0U; Index < CANTP_TXNSDU_COUNT; Index++)
    {
        if (CanTp_TxRuntime[Index].State == CANTP_TX_STATE_SEND_CF)
        {
            (void)CanTp_SendNextConsecutiveFrame(&CanTp_TxRuntime[Index]);
        }
    }
}

/*********************************************************************************************************************/
/*----------------------------------------------Private Functions-----------------------------------------------------*/
/*********************************************************************************************************************/

static Std_ReturnType CanTp_SendSingleFrame(
    const CanTp_TxNsduConfigType* TxConfig,
    CanTp_TxRuntimeType* TxRuntime,
    const PduInfoType* CanTpTxInfoPtr
)
{
    PduInfoType CanIfPduInfo;

    if ((TxConfig == NULL_PTR) || (TxRuntime == NULL_PTR) || (CanTpTxInfoPtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    memset(TxRuntime->CanFrameBuffer, 0x00U, CANTP_CAN_FRAME_LENGTH);

    TxRuntime->CanFrameBuffer[0] =
        (uint8)(CANTP_PCI_TYPE_SF |
        ((uint8)(CanTpTxInfoPtr->SduLength >> 8U) & CANTP_PCI_LENGTH_MASK));

    TxRuntime->CanFrameBuffer[1] =
        (uint8)(CanTpTxInfoPtr->SduLength & 0xFFU);

    memcpy(
        &TxRuntime->CanFrameBuffer[CANTP_LENGTH_PCI_LENGTH],
        CanTpTxInfoPtr->SduDataPtr,
        CanTpTxInfoPtr->SduLength
    );

    CanIfPduInfo.SduDataPtr = TxRuntime->CanFrameBuffer;
    CanIfPduInfo.SduLength  = CANTP_CAN_FRAME_LENGTH;

    TxRuntime->State = CANTP_TX_STATE_WAIT_TX_CONFIRMATION;
    TxRuntime->TotalLength = 0U;

    CANTP_DEBUG_PRINTF(
        "[CanTp][TX] Frame=SF CanIfTxNpduId=%u TotalLength=%u\r\n",
        TxConfig->CanIfTxNpduId,
        CanTpTxInfoPtr->SduLength
    );

    CANTP_DEBUG_PRINT_PDU(
        "[CanTp][TX] N-PDU",
        &CanIfPduInfo
    );

    return CanIf_Transmit(
        TxConfig->CanIfTxNpduId,
        &CanIfPduInfo
    );
}

static Std_ReturnType CanTp_SendFirstFrame(
    const CanTp_TxNsduConfigType* TxConfig,
    CanTp_TxRuntimeType* TxRuntime
)
{
    PduInfoType CanIfPduInfo;

    if ((TxConfig == NULL_PTR) || (TxRuntime == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (TxRuntime->TotalLength <= CANTP_FF_DATA_LENGTH)
    {
        return E_NOT_OK;
    }

    memset(TxRuntime->CanFrameBuffer, 0x00U, CANTP_CAN_FRAME_LENGTH);

    TxRuntime->CanFrameBuffer[0] =
        (uint8)(CANTP_PCI_TYPE_FF |
        ((TxRuntime->TotalLength >> 8U) & CANTP_PCI_LENGTH_MASK));

    TxRuntime->CanFrameBuffer[1] =
        (uint8)(TxRuntime->TotalLength & 0xFFU);

    memcpy(
        &TxRuntime->CanFrameBuffer[CANTP_LENGTH_PCI_LENGTH],
        TxRuntime->Buffer,
        CANTP_FF_DATA_LENGTH
    );

    CanIfPduInfo.SduDataPtr = TxRuntime->CanFrameBuffer;
    CanIfPduInfo.SduLength  = CANTP_CAN_FRAME_LENGTH;

    CANTP_DEBUG_PRINTF(
        "[CanTp][TX] Frame=FF CanIfTxNpduId=%u TotalLength=%u\r\n",
        TxConfig->CanIfTxNpduId,
        TxRuntime->TotalLength
    );

    CANTP_DEBUG_PRINT_PDU(
        "[CanTp][TX] N-PDU",
        &CanIfPduInfo
    );

    return CanIf_Transmit(
        TxConfig->CanIfTxNpduId,
        &CanIfPduInfo
    );
}

static Std_ReturnType CanTp_SendNextConsecutiveFrame(
    CanTp_TxRuntimeType* TxRuntime
)
{
    PduInfoType CanIfPduInfo;
    uint16 RemainingLength;
    uint8 CopyLength;

    if (TxRuntime == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (TxRuntime->TransmittedLength >= TxRuntime->TotalLength)
    {
        PduR_CanTpTxConfirmation(
            TxRuntime->PduRTxPduId,
            E_OK
        );

        CanTp_ResetTxRuntime(TxRuntime);
        return E_OK;
    }

    if ((TxRuntime->BlockSize != 0U) &&
        (TxRuntime->BlockCounter >= TxRuntime->BlockSize))
    {
        TxRuntime->State = CANTP_TX_STATE_WAIT_FC;
        return E_OK;
    }

    RemainingLength =
        TxRuntime->TotalLength - TxRuntime->TransmittedLength;

    if (RemainingLength > CANTP_CF_DATA_LENGTH)
    {
        CopyLength = CANTP_CF_DATA_LENGTH;
    }
    else
    {
        CopyLength = (uint8)RemainingLength;
    }

    memset(TxRuntime->CanFrameBuffer, 0x00U, CANTP_CAN_FRAME_LENGTH);

    TxRuntime->CanFrameBuffer[0] =
        (uint8)(CANTP_PCI_TYPE_CF |
        (TxRuntime->NextSequenceNumber & CANTP_PCI_SN_MASK));

    memcpy(
        &TxRuntime->CanFrameBuffer[1],
        &TxRuntime->Buffer[TxRuntime->TransmittedLength],
        CopyLength
    );

    CanIfPduInfo.SduDataPtr = TxRuntime->CanFrameBuffer;
    CanIfPduInfo.SduLength  = CANTP_CAN_FRAME_LENGTH;

    CANTP_DEBUG_PRINTF(
        "[CanTp][TX] Frame=CF CanIfTxNpduId=%u SN=%u CopyLength=%u\r\n",
        TxRuntime->CanIfTxPduId,
        TxRuntime->NextSequenceNumber,
        CopyLength
    );

    CANTP_DEBUG_PRINT_PDU(
        "[CanTp][TX] N-PDU",
        &CanIfPduInfo
    );

    if (CanIf_Transmit(TxRuntime->CanIfTxPduId, &CanIfPduInfo) == E_OK)
    {
        TxRuntime->TransmittedLength += CopyLength;

        TxRuntime->NextSequenceNumber =
            (uint8)((TxRuntime->NextSequenceNumber + 1U) & CANTP_PCI_SN_MASK);

        TxRuntime->BlockCounter++;
        TxRuntime->State = CANTP_TX_STATE_WAIT_TX_CONFIRMATION;

        return E_OK;
    }

    return E_NOT_OK;
}

static void CanTp_HandleSingleFrame(
    const CanTp_RxNsduConfigType* RxConfig,
    CanTp_RxRuntimeType* RxRuntime,
    const PduInfoType* PduInfoPtr
)
{
    PduInfoType CanTpRxInfo;
    uint16 PayloadLength;

    if ((RxConfig == NULL_PTR) || (RxRuntime == NULL_PTR) || (PduInfoPtr == NULL_PTR))
    {
        return;
    }

    if (PduInfoPtr->SduLength < CANTP_LENGTH_PCI_LENGTH)
    {
        return;
    }

    PayloadLength =
        (uint16)(((uint16)(PduInfoPtr->SduDataPtr[0] & CANTP_PCI_LENGTH_MASK) << 8U) |
                 ((uint16)PduInfoPtr->SduDataPtr[1]));

    if (PayloadLength > CANTP_SF_MAX_PAYLOAD_LENGTH)
    {
        return;
    }

    if ((PduLengthType)(PayloadLength + CANTP_LENGTH_PCI_LENGTH) > PduInfoPtr->SduLength)
    {
        return;
    }

    memcpy(
        RxRuntime->Buffer,
        &PduInfoPtr->SduDataPtr[CANTP_LENGTH_PCI_LENGTH],
        PayloadLength
    );

    CanTpRxInfo.SduDataPtr = RxRuntime->Buffer;
    CanTpRxInfo.SduLength  = PayloadLength;

    CANTP_DEBUG_PRINTF(
        "[CanTp][RX] Complete Type=SF PduRRxPduId=%u\r\n",
        RxConfig->PduRRxPduId
    );

    CANTP_DEBUG_PRINT_PDU(
        "[CanTp][RX] N-SDU",
        &CanTpRxInfo
    );

    PduR_CanTpRxIndication(
        RxConfig->PduRRxPduId,
        &CanTpRxInfo
    );
}

static void CanTp_HandleFirstFrame(
    const CanTp_RxNsduConfigType* RxConfig,
    CanTp_RxRuntimeType* RxRuntime,
    const PduInfoType* PduInfoPtr
)
{
    uint16 TotalLength;

    if ((RxConfig == NULL_PTR) || (RxRuntime == NULL_PTR) || (PduInfoPtr == NULL_PTR))
    {
        return;
    }

    if (RxRuntime->State != CANTP_RX_STATE_IDLE)
    {
        (void)CanTp_SendFlowControl(
            RxConfig,
            RxRuntime,
            CANTP_FC_STATUS_WAIT
        );
        return;
    }

    if (PduInfoPtr->SduLength < CANTP_CAN_FRAME_LENGTH)
    {
        return;
    }

    TotalLength =
        (uint16)(((uint16)(PduInfoPtr->SduDataPtr[0] & CANTP_PCI_LENGTH_MASK) << 8U) |
                 ((uint16)PduInfoPtr->SduDataPtr[1]));

    if ((TotalLength == 0U) ||
        (TotalLength <= CANTP_FF_DATA_LENGTH) ||
        (TotalLength > RxConfig->RxBufferSize))
    {
        (void)CanTp_SendFlowControl(
            RxConfig,
            RxRuntime,
            CANTP_FC_STATUS_OVERFLOW
        );
        return;
    }

    memset(RxRuntime->Buffer, 0x00U, CANTP_RX_BUFFER_SIZE);

    memcpy(
        RxRuntime->Buffer,
        &PduInfoPtr->SduDataPtr[CANTP_LENGTH_PCI_LENGTH],
        CANTP_FF_DATA_LENGTH
    );

    RxRuntime->State                  = CANTP_RX_STATE_RECEIVING;
    RxRuntime->CanTpRxNsduId          = RxConfig->CanTpRxNsduId;
    RxRuntime->PduRRxPduId            = RxConfig->PduRRxPduId;
    RxRuntime->CanIfTxFcPduId         = RxConfig->CanIfTxFcPduId;
    RxRuntime->TotalLength            = TotalLength;
    RxRuntime->ReceivedLength         = CANTP_FF_DATA_LENGTH;
    RxRuntime->ExpectedSequenceNumber = 1U;
    RxRuntime->BlockCounter           = 0U;

    CANTP_DEBUG_PRINTF(
        "[CanTp][RX] Frame=FF CanTpRxNsduId=%u PduRRxPduId=%u TotalLength=%u\r\n",
        RxConfig->CanTpRxNsduId,
        RxConfig->PduRRxPduId,
        TotalLength
    );

    (void)CanTp_SendFlowControl(
        RxConfig,
        RxRuntime,
        CANTP_FC_STATUS_CTS
    );
}

static void CanTp_HandleConsecutiveFrame(
    const CanTp_RxNsduConfigType* RxConfig,
    CanTp_RxRuntimeType* RxRuntime,
    const PduInfoType* PduInfoPtr
)
{
    PduInfoType CanTpRxInfo;
    uint8 SequenceNumber;
    uint16 RemainingLength;
    uint8 CopyLength;

    if ((RxConfig == NULL_PTR) || (RxRuntime == NULL_PTR) || (PduInfoPtr == NULL_PTR))
    {
        return;
    }

    if (RxRuntime->State != CANTP_RX_STATE_RECEIVING)
    {
        return;
    }

    if (PduInfoPtr->SduLength < 1U)
    {
        return;
    }

    SequenceNumber = PduInfoPtr->SduDataPtr[0] & CANTP_PCI_SN_MASK;

    if (SequenceNumber != RxRuntime->ExpectedSequenceNumber)
    {
        CANTP_DEBUG_PRINTF(
            "[CanTp][RX-ERR] SN mismatch Expected=%u Received=%u ReceivedLength=%u TotalLength=%u\r\n",
            RxRuntime->ExpectedSequenceNumber,
            SequenceNumber,
            RxRuntime->ReceivedLength,
            RxRuntime->TotalLength
        );

        CanTp_ResetRxRuntime(RxRuntime);
        return;
    }

    RemainingLength =
        RxRuntime->TotalLength - RxRuntime->ReceivedLength;

    if (RemainingLength > CANTP_CF_DATA_LENGTH)
    {
        CopyLength = CANTP_CF_DATA_LENGTH;
    }
    else
    {
        CopyLength = (uint8)RemainingLength;
    }

    if ((RxRuntime->ReceivedLength + CopyLength) > RxRuntime->TotalLength)
    {
        CanTp_ResetRxRuntime(RxRuntime);
        return;
    }

    if ((RxRuntime->ReceivedLength + CopyLength) > RxConfig->RxBufferSize)
    {
        CanTp_ResetRxRuntime(RxRuntime);
        return;
    }

    memcpy(
        &RxRuntime->Buffer[RxRuntime->ReceivedLength],
        &PduInfoPtr->SduDataPtr[1],
        CopyLength
    );

    RxRuntime->ReceivedLength += CopyLength;

    RxRuntime->ExpectedSequenceNumber =
        (uint8)((RxRuntime->ExpectedSequenceNumber + 1U) & CANTP_PCI_SN_MASK);

    RxRuntime->BlockCounter++;

    if (RxRuntime->ReceivedLength >= RxRuntime->TotalLength)
    {
        CanTpRxInfo.SduDataPtr = RxRuntime->Buffer;
        CanTpRxInfo.SduLength  = RxRuntime->TotalLength;

        CANTP_DEBUG_PRINTF(
            "[CanTp][RX] Complete Type=MF PduRRxPduId=%u\r\n",
            RxRuntime->PduRRxPduId
        );

        CANTP_DEBUG_PRINT_PDU(
            "[CanTp][RX] N-SDU",
            &CanTpRxInfo
        );

        PduR_CanTpRxIndication(
            RxRuntime->PduRRxPduId,
            &CanTpRxInfo
        );

        CanTp_ResetRxRuntime(RxRuntime);
        return;
    }

    if ((RxConfig->BlockSize != 0U) &&
        (RxRuntime->BlockCounter >= RxConfig->BlockSize))
    {
        RxRuntime->BlockCounter = 0U;

        (void)CanTp_SendFlowControl(
            RxConfig,
            RxRuntime,
            CANTP_FC_STATUS_CTS
        );
    }
}

static Std_ReturnType CanTp_SendFlowControl(
    const CanTp_RxNsduConfigType* RxConfig,
    CanTp_RxRuntimeType* RxRuntime,
    uint8 FlowStatus
)
{
    PduInfoType CanIfPduInfo;

    if ((RxConfig == NULL_PTR) || (RxRuntime == NULL_PTR))
    {
        return E_NOT_OK;
    }

    memset(RxRuntime->FcFrameBuffer, 0x00U, CANTP_CAN_FRAME_LENGTH);

    RxRuntime->FcFrameBuffer[0] =
        (uint8)(CANTP_PCI_TYPE_FC |
        (FlowStatus & CANTP_PCI_LENGTH_MASK));

    RxRuntime->FcFrameBuffer[1] = RxConfig->BlockSize;
    RxRuntime->FcFrameBuffer[2] = RxConfig->STmin;

    CanIfPduInfo.SduDataPtr = RxRuntime->FcFrameBuffer;
    CanIfPduInfo.SduLength  = CANTP_CAN_FRAME_LENGTH;

    CANTP_DEBUG_PRINTF(
        "[CanTp][TX] Frame=FC CanIfTxNpduId=%u FlowStatus=%u\r\n",
        RxConfig->CanIfTxFcPduId,
        FlowStatus
    );

    CANTP_DEBUG_PRINT_PDU(
        "[CanTp][TX] N-PDU",
        &CanIfPduInfo
    );

    return CanIf_Transmit(
        RxConfig->CanIfTxFcPduId,
        &CanIfPduInfo
    );
}

static void CanTp_HandleFlowControl(
    const CanTp_RxNsduConfigType* RxConfig,
    const PduInfoType* PduInfoPtr
)
{
    CanTp_TxRuntimeType* TxRuntime;
    uint8 FlowStatus;
    uint8 BlockSize;
    uint8 STmin;

    if ((RxConfig == NULL_PTR) || (PduInfoPtr == NULL_PTR))
    {
        return;
    }

    TxRuntime = CanTp_FindTxRuntimeWaitingFc(RxConfig->CanTpRxNsduId);

    if (TxRuntime == NULL_PTR)
    {
        return;
    }

    if (PduInfoPtr->SduLength < 3U)
    {
        return;
    }

    if (TxRuntime->State != CANTP_TX_STATE_WAIT_FC)
    {
        return;
    }

    FlowStatus = PduInfoPtr->SduDataPtr[0] & CANTP_PCI_LENGTH_MASK;
    BlockSize  = PduInfoPtr->SduDataPtr[1];
    STmin      = PduInfoPtr->SduDataPtr[2];

    CANTP_DEBUG_PRINTF(
        "[CanTp][RX] Frame=FC FlowStatus=%u BlockSize=%u STmin=%u\r\n",
        FlowStatus,
        BlockSize,
        STmin
    );

    if (FlowStatus == CANTP_FC_STATUS_CTS)
    {
        TxRuntime->BlockSize      = BlockSize;
        TxRuntime->BlockCounter   = 0U;
        TxRuntime->STmin          = STmin;
        TxRuntime->WaitFrameCount = 0U;
        TxRuntime->State          = CANTP_TX_STATE_SEND_CF;
    }
    else if (FlowStatus == CANTP_FC_STATUS_WAIT)
    {
        TxRuntime->WaitFrameCount++;

        if (TxRuntime->WaitFrameCount > CANTP_MAX_WAIT_FRAME_COUNT)
        {
            PduR_CanTpTxConfirmation(
                TxRuntime->PduRTxPduId,
                E_NOT_OK
            );

            CanTp_ResetTxRuntime(TxRuntime);
        }
        else
        {
            TxRuntime->State = CANTP_TX_STATE_WAIT_FC;
        }
    }
    else
    {
        PduR_CanTpTxConfirmation(
            TxRuntime->PduRTxPduId,
            E_NOT_OK
        );

        CanTp_ResetTxRuntime(TxRuntime);
    }
}

static const CanTp_TxNsduConfigType* CanTp_FindTxNsduConfig(
    PduIdType CanTpTxNsduId
)
{
    uint8 Index;

    for (Index = 0U; Index < CANTP_TXNSDU_COUNT; Index++)
    {
        if (CanTp_TxNsduConfig[Index].CanTpTxNsduId == CanTpTxNsduId)
        {
            return &CanTp_TxNsduConfig[Index];
        }
    }

    return NULL_PTR;
}

static const CanTp_RxNsduConfigType* CanTp_FindRxNsduConfigByNpduId(
    PduIdType CanTpRxNpduId
)
{
    uint8 Index;

    for (Index = 0U; Index < CANTP_RXNSDU_COUNT; Index++)
    {
        if (CanTp_RxNsduConfig[Index].CanIfRxNpduId == CanTpRxNpduId)
        {
            return &CanTp_RxNsduConfig[Index];
        }
    }

    return NULL_PTR;
}

static CanTp_TxRuntimeType* CanTp_GetTxRuntimeByTxNsduId(
    PduIdType CanTpTxNsduId
)
{
    uint8 Index;

    for (Index = 0U; Index < CANTP_TXNSDU_COUNT; Index++)
    {
        if (CanTp_TxNsduConfig[Index].CanTpTxNsduId == CanTpTxNsduId)
        {
            return &CanTp_TxRuntime[Index];
        }
    }

    return NULL_PTR;
}

static CanTp_TxRuntimeType* CanTp_GetTxRuntimeByTxNpduId(
    PduIdType CanTpTxNpduId
)
{
    uint8 Index;

    for (Index = 0U; Index < CANTP_TXNSDU_COUNT; Index++)
    {
        /*
         * CanIf returns the Tx N-PDU handle associated with this CanTp channel.
         * In this project the CanTp Tx N-PDU and CanIf Tx L-PDU IDs are aligned.
         */
        if (CanTp_TxNsduConfig[Index].CanIfTxNpduId == CanTpTxNpduId)
        {
            return &CanTp_TxRuntime[Index];
        }
    }

    return NULL_PTR;
}

static CanTp_TxRuntimeType* CanTp_FindTxRuntimeWaitingFc(
    PduIdType ExpectedRxNsduId
)
{
    uint8 Index;

    for (Index = 0U; Index < CANTP_TXNSDU_COUNT; Index++)
    {
        if ((CanTp_TxRuntime[Index].State == CANTP_TX_STATE_WAIT_FC) &&
            (CanTp_TxRuntime[Index].ExpectedRxNsduId == ExpectedRxNsduId))
        {
            return &CanTp_TxRuntime[Index];
        }
    }

    return NULL_PTR;
}

static CanTp_RxRuntimeType* CanTp_GetRxRuntimeByRxNsduId(
    PduIdType CanTpRxNsduId
)
{
    uint8 Index;

    for (Index = 0U; Index < CANTP_RXNSDU_COUNT; Index++)
    {
        if (CanTp_RxNsduConfig[Index].CanTpRxNsduId == CanTpRxNsduId)
        {
            return &CanTp_RxRuntime[Index];
        }
    }

    return NULL_PTR;
}

static void CanTp_ResetTxRuntime(
    CanTp_TxRuntimeType* TxRuntime
)
{
    if (TxRuntime == NULL_PTR)
    {
        return;
    }

    memset(TxRuntime, 0x00U, sizeof(*TxRuntime));
    TxRuntime->State = CANTP_TX_STATE_IDLE;
}

static void CanTp_ResetRxRuntime(
    CanTp_RxRuntimeType* RxRuntime
)
{
    if (RxRuntime == NULL_PTR)
    {
        return;
    }

    memset(RxRuntime, 0x00U, sizeof(*RxRuntime));
    RxRuntime->State = CANTP_RX_STATE_IDLE;
}
