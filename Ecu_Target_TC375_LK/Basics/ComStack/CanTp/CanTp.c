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
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

#define CANTP_DEBUG_LOG_PACKET(Direction, PduId, PduInfoPtr)           \
    do                                                                 \
    {                                                                  \
        CANTP_DEBUG_PRINTF(                                            \
            "[CanTp][%s] PduId=%u",                                    \
            (Direction),                                               \
            (unsigned int)(PduId)                                      \
        );                                                             \
        CANTP_DEBUG_PRINT_PDU("", (PduInfoPtr));                      \
    } while (0)

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

static CanTp_TxRuntimeType CanTp_TxRuntime;
static CanTp_RxRuntimeType CanTp_RxRuntime;

/*********************************************************************************************************************/
/*----------------------------------------------Private Functions-----------------------------------------------------*/
/*********************************************************************************************************************/

static const CanTp_TxNsduConfigType* CanTp_FindTxNsduConfig(
    PduIdType CanTpTxNsduId
);

static const CanTp_RxNsduConfigType* CanTp_FindRxNsduConfigByNpduId(
    PduIdType CanTpRxNpduId
);

static Std_ReturnType CanTp_SendSingleFrame(
    const CanTp_TxNsduConfigType* TxConfig,
    const PduInfoType* CanTpTxInfoPtr
);

static Std_ReturnType CanTp_SendFirstFrame(
    const CanTp_TxNsduConfigType* TxConfig
);

static Std_ReturnType CanTp_SendNextConsecutiveFrame(void);

static Std_ReturnType CanTp_SendFlowControl(
    const CanTp_RxNsduConfigType* RxConfig,
    uint8 FlowStatus
);

static void CanTp_HandleSingleFrame(
    const CanTp_RxNsduConfigType* RxConfig,
    const PduInfoType* PduInfoPtr
);

static void CanTp_HandleFirstFrame(
    const CanTp_RxNsduConfigType* RxConfig,
    const PduInfoType* PduInfoPtr
);

static void CanTp_HandleConsecutiveFrame(
    const CanTp_RxNsduConfigType* RxConfig,
    const PduInfoType* PduInfoPtr
);

static void CanTp_HandleFlowControl(
    const PduInfoType* PduInfoPtr
);

static void CanTp_ResetTxRuntime(void);
static void CanTp_ResetRxRuntime(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void CanTp_Init(void)
{
    CanTp_ResetTxRuntime();
    CanTp_ResetRxRuntime();
}

Std_ReturnType CanTp_Transmit(
    PduIdType CanTpTxSduId,
    const PduInfoType* CanTpTxInfoPtr
)
{
    const CanTp_TxNsduConfigType* TxConfig;

    if ((CanTpTxInfoPtr == NULL_PTR) ||
        (CanTpTxInfoPtr->SduDataPtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (CanTpTxInfoPtr->SduLength == 0U)
    {
        return E_NOT_OK;
    }

    if (CanTp_TxRuntime.State != CANTP_TX_STATE_IDLE)
    {
        return E_NOT_OK;
    }

    TxConfig = CanTp_FindTxNsduConfig(CanTpTxSduId);

    if (TxConfig == NULL_PTR)
    {
        return E_NOT_OK;
    }

    CANTP_DEBUG_LOG_PACKET(
        "TX-REQ",
        CanTpTxSduId,
        CanTpTxInfoPtr
    );

    if (CanTpTxInfoPtr->SduLength <= CANTP_SF_MAX_PAYLOAD_LENGTH)
    {
        CanTp_TxRuntime.CanTpTxNsduId = TxConfig->CanTpTxNsduId;
        CanTp_TxRuntime.PduRTxPduId   = TxConfig->PduRTxPduId;
        CanTp_TxRuntime.CanIfTxPduId  = TxConfig->CanIfTxNpduId;

        return CanTp_SendSingleFrame(
            TxConfig,
            CanTpTxInfoPtr
        );
    }

    if (CanTpTxInfoPtr->SduLength > CANTP_TX_BUFFER_SIZE)
    {
        return E_NOT_OK;
    }

    memcpy(
        CanTp_TxRuntime.Buffer,
        CanTpTxInfoPtr->SduDataPtr,
        CanTpTxInfoPtr->SduLength
    );

    CanTp_TxRuntime.CanTpTxNsduId      = TxConfig->CanTpTxNsduId;
    CanTp_TxRuntime.PduRTxPduId        = TxConfig->PduRTxPduId;
    CanTp_TxRuntime.CanIfTxPduId       = TxConfig->CanIfTxNpduId;

    CanTp_TxRuntime.TotalLength        = CanTpTxInfoPtr->SduLength;
    CanTp_TxRuntime.TransmittedLength  = CANTP_FF_DATA_LENGTH;

    CanTp_TxRuntime.NextSequenceNumber = 1U;
    CanTp_TxRuntime.BlockSize          = 0U;
    CanTp_TxRuntime.BlockCounter       = 0U;
    CanTp_TxRuntime.STmin              = 0U;
    CanTp_TxRuntime.WaitFrameCount     = 0U;

    if (CanTp_SendFirstFrame(TxConfig) != E_OK)
    {
        CanTp_ResetTxRuntime();
        return E_NOT_OK;
    }

    CanTp_TxRuntime.State = CANTP_TX_STATE_WAIT_FC;

    return E_OK;
}

void CanTp_RxIndication(
    PduIdType CanTpRxNpduId,
    const PduInfoType* PduInfoPtr
)
{
    const CanTp_RxNsduConfigType* RxConfig;
    uint8 PciType;

    if ((PduInfoPtr == NULL_PTR) ||
        (PduInfoPtr->SduDataPtr == NULL_PTR) ||
        (PduInfoPtr->SduLength == 0U))
    {
        return;
    }

    CANTP_DEBUG_LOG_PACKET(
        "RX",
        CanTpRxNpduId,
        PduInfoPtr
    );

    PciType = PduInfoPtr->SduDataPtr[0] & CANTP_PCI_TYPE_MASK;

    if (PciType == CANTP_PCI_TYPE_FC)
    {
        CanTp_HandleFlowControl(PduInfoPtr);
        return;
    }

    RxConfig = CanTp_FindRxNsduConfigByNpduId(CanTpRxNpduId);

    if (RxConfig == NULL_PTR)
    {
        return;
    }

    switch (PciType)
    {
        case CANTP_PCI_TYPE_SF:
        {
            CanTp_HandleSingleFrame(RxConfig, PduInfoPtr);
            break;
        }

        case CANTP_PCI_TYPE_FF:
        {
            CanTp_HandleFirstFrame(RxConfig, PduInfoPtr);
            break;
        }

        case CANTP_PCI_TYPE_CF:
        {
            CanTp_HandleConsecutiveFrame(RxConfig, PduInfoPtr);
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
    CANTP_DEBUG_PRINTF(
        "[CanTp][TX-CNF] CanTpTxNPduId=%u Result=%u State=%u\r\n",
        (unsigned int)CanTpTxNPduId,
        (unsigned int)result,
        (unsigned int)CanTp_TxRuntime.State
    );

    if (CanTp_TxRuntime.State == CANTP_TX_STATE_IDLE)
    {
        return;
    }

    if (result != E_OK)
    {
        PduR_CanTpTxConfirmation(
            CanTp_TxRuntime.PduRTxPduId,
            E_NOT_OK
        );

        CanTp_ResetTxRuntime();
        return;
    }

    if (CanTp_TxRuntime.TotalLength == 0U)
    {
        PduR_CanTpTxConfirmation(
            CanTp_TxRuntime.PduRTxPduId,
            E_OK
        );

        CanTp_ResetTxRuntime();
        return;
    }

    if (CanTp_TxRuntime.State == CANTP_TX_STATE_WAIT_TX_CONFIRMATION)
    {
        if (CanTp_TxRuntime.TransmittedLength >= CanTp_TxRuntime.TotalLength)
        {
            PduR_CanTpTxConfirmation(
                CanTp_TxRuntime.PduRTxPduId,
                E_OK
            );

            CanTp_ResetTxRuntime();
        }
        else if ((CanTp_TxRuntime.BlockSize != 0U) &&
                 (CanTp_TxRuntime.BlockCounter >= CanTp_TxRuntime.BlockSize))
        {
            CanTp_TxRuntime.State = CANTP_TX_STATE_WAIT_FC;
        }
        else
        {
            CanTp_TxRuntime.State = CANTP_TX_STATE_SEND_CF;
        }
    }
}

void CanTp_MainFunction(void)
{
    if (CanTp_TxRuntime.State == CANTP_TX_STATE_SEND_CF)
    {
        (void)CanTp_SendNextConsecutiveFrame();
    }
}

/*********************************************************************************************************************/
/*----------------------------------------------Private Functions-----------------------------------------------------*/
/*********************************************************************************************************************/

static Std_ReturnType CanTp_SendSingleFrame(
    const CanTp_TxNsduConfigType* TxConfig,
    const PduInfoType* CanTpTxInfoPtr
)
{
    PduInfoType CanIfPduInfo;

    memset(CanTp_TxRuntime.CanFrameBuffer, 0x00U, CANTP_CAN_FRAME_LENGTH);

    CanTp_TxRuntime.CanFrameBuffer[0] =
        (uint8)(CANTP_PCI_TYPE_SF |
        ((uint8)CanTpTxInfoPtr->SduLength & CANTP_PCI_LENGTH_MASK));

    memcpy(
        &CanTp_TxRuntime.CanFrameBuffer[1],
        CanTpTxInfoPtr->SduDataPtr,
        CanTpTxInfoPtr->SduLength
    );

    CanIfPduInfo.SduDataPtr = CanTp_TxRuntime.CanFrameBuffer;
    CanIfPduInfo.SduLength  = CANTP_CAN_FRAME_LENGTH;

    CanTp_TxRuntime.State = CANTP_TX_STATE_WAIT_TX_CONFIRMATION;
    CanTp_TxRuntime.TotalLength = 0U;

    CANTP_DEBUG_LOG_PACKET(
        "TX",
        TxConfig->CanIfTxNpduId,
        &CanIfPduInfo
    );

    return CanIf_Transmit(
        TxConfig->CanIfTxNpduId,
        &CanIfPduInfo
    );
}

static Std_ReturnType CanTp_SendFirstFrame(
    const CanTp_TxNsduConfigType* TxConfig
)
{
    PduInfoType CanIfPduInfo;

    if (CanTp_TxRuntime.TotalLength < CANTP_FF_DATA_LENGTH)
    {
        return E_NOT_OK;
    }

    memset(CanTp_TxRuntime.CanFrameBuffer, 0x00U, CANTP_CAN_FRAME_LENGTH);

    CanTp_TxRuntime.CanFrameBuffer[0] =
        (uint8)(CANTP_PCI_TYPE_FF |
        ((CanTp_TxRuntime.TotalLength >> 8U) & CANTP_PCI_LENGTH_MASK));

    CanTp_TxRuntime.CanFrameBuffer[1] =
        (uint8)(CanTp_TxRuntime.TotalLength & 0xFFU);

    memcpy(
        &CanTp_TxRuntime.CanFrameBuffer[2],
        CanTp_TxRuntime.Buffer,
        CANTP_FF_DATA_LENGTH
    );

    CanIfPduInfo.SduDataPtr = CanTp_TxRuntime.CanFrameBuffer;
    CanIfPduInfo.SduLength  = CANTP_CAN_FRAME_LENGTH;

    CANTP_DEBUG_LOG_PACKET(
        "TX",
        TxConfig->CanIfTxNpduId,
        &CanIfPduInfo
    );

    return CanIf_Transmit(
        TxConfig->CanIfTxNpduId,
        &CanIfPduInfo
    );
}

static Std_ReturnType CanTp_SendNextConsecutiveFrame(void)
{
    PduInfoType CanIfPduInfo;
    uint16 RemainingLength;
    uint8 CopyLength;

    if (CanTp_TxRuntime.TransmittedLength >= CanTp_TxRuntime.TotalLength)
    {
        PduR_CanTpTxConfirmation(
            CanTp_TxRuntime.PduRTxPduId,
            E_OK
        );

        CanTp_ResetTxRuntime();
        return E_OK;
    }

    if ((CanTp_TxRuntime.BlockSize != 0U) &&
        (CanTp_TxRuntime.BlockCounter >= CanTp_TxRuntime.BlockSize))
    {
        CanTp_TxRuntime.State = CANTP_TX_STATE_WAIT_FC;
        return E_OK;
    }

    RemainingLength =
        CanTp_TxRuntime.TotalLength - CanTp_TxRuntime.TransmittedLength;

    if (RemainingLength > CANTP_CF_DATA_LENGTH)
    {
        CopyLength = CANTP_CF_DATA_LENGTH;
    }
    else
    {
        CopyLength = (uint8)RemainingLength;
    }

    memset(CanTp_TxRuntime.CanFrameBuffer, 0x00U, CANTP_CAN_FRAME_LENGTH);

    CanTp_TxRuntime.CanFrameBuffer[0] =
        (uint8)(CANTP_PCI_TYPE_CF |
        (CanTp_TxRuntime.NextSequenceNumber & CANTP_PCI_SN_MASK));

    memcpy(
        &CanTp_TxRuntime.CanFrameBuffer[1],
        &CanTp_TxRuntime.Buffer[CanTp_TxRuntime.TransmittedLength],
        CopyLength
    );

    CanIfPduInfo.SduDataPtr = CanTp_TxRuntime.CanFrameBuffer;
    CanIfPduInfo.SduLength  = CANTP_CAN_FRAME_LENGTH;

    CANTP_DEBUG_LOG_PACKET(
        "TX",
        CanTp_TxRuntime.CanIfTxPduId,
        &CanIfPduInfo
    );

    if (CanIf_Transmit(CanTp_TxRuntime.CanIfTxPduId, &CanIfPduInfo) == E_OK)
    {
        CanTp_TxRuntime.TransmittedLength += CopyLength;

        CanTp_TxRuntime.NextSequenceNumber =
            (uint8)((CanTp_TxRuntime.NextSequenceNumber + 1U) & CANTP_PCI_SN_MASK);

        CanTp_TxRuntime.BlockCounter++;
        CanTp_TxRuntime.State = CANTP_TX_STATE_WAIT_TX_CONFIRMATION;

        return E_OK;
    }

    return E_NOT_OK;
}

static void CanTp_HandleSingleFrame(
    const CanTp_RxNsduConfigType* RxConfig,
    const PduInfoType* PduInfoPtr
)
{
    PduInfoType CanTpRxInfo;
    uint8 PayloadLength;

    PayloadLength = PduInfoPtr->SduDataPtr[0] & CANTP_PCI_LENGTH_MASK;

    if (PayloadLength > CANTP_SF_MAX_PAYLOAD_LENGTH)
    {
        return;
    }

    if ((PduLengthType)(PayloadLength + 1U) > PduInfoPtr->SduLength)
    {
        return;
    }

    memcpy(
        CanTp_RxRuntime.Buffer,
        &PduInfoPtr->SduDataPtr[1],
        PayloadLength
    );

    CanTpRxInfo.SduDataPtr = CanTp_RxRuntime.Buffer;
    CanTpRxInfo.SduLength  = PayloadLength;

    CANTP_DEBUG_LOG_PACKET(
        "RX-COMPLETE",
        RxConfig->CanTpRxNsduId,
        &CanTpRxInfo
    );

    PduR_CanTpRxIndication(
        RxConfig->PduRRxPduId,
        &CanTpRxInfo
    );
}

static void CanTp_HandleFirstFrame(
    const CanTp_RxNsduConfigType* RxConfig,
    const PduInfoType* PduInfoPtr
)
{
    uint16 TotalLength;

    if (CanTp_RxRuntime.State != CANTP_RX_STATE_IDLE)
    {
        (void)CanTp_SendFlowControl(
            RxConfig,
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

    if ((TotalLength == 0U) || (TotalLength > RxConfig->RxBufferSize))
    {
        (void)CanTp_SendFlowControl(
            RxConfig,
            CANTP_FC_STATUS_OVERFLOW
        );
        return;
    }

    memset(CanTp_RxRuntime.Buffer, 0x00U, CANTP_RX_BUFFER_SIZE);

    memcpy(
        CanTp_RxRuntime.Buffer,
        &PduInfoPtr->SduDataPtr[2],
        CANTP_FF_DATA_LENGTH
    );

    CanTp_RxRuntime.State                  = CANTP_RX_STATE_RECEIVING;
    CanTp_RxRuntime.CanTpRxNsduId          = RxConfig->CanTpRxNsduId;
    CanTp_RxRuntime.PduRRxPduId            = RxConfig->PduRRxPduId;
    CanTp_RxRuntime.CanIfTxFcPduId         = RxConfig->CanIfTxFcPduId;
    CanTp_RxRuntime.TotalLength            = TotalLength;
    CanTp_RxRuntime.ReceivedLength         = CANTP_FF_DATA_LENGTH;
    CanTp_RxRuntime.ExpectedSequenceNumber = 1U;
    CanTp_RxRuntime.BlockCounter           = 0U;

    (void)CanTp_SendFlowControl(
        RxConfig,
        CANTP_FC_STATUS_CTS
    );
}

static void CanTp_HandleConsecutiveFrame(
    const CanTp_RxNsduConfigType* RxConfig,
    const PduInfoType* PduInfoPtr
)
{
    PduInfoType CanTpRxInfo;
    uint8 SequenceNumber;
    uint16 RemainingLength;
    uint8 CopyLength;

    if (CanTp_RxRuntime.State != CANTP_RX_STATE_RECEIVING)
    {
        return;
    }

    if (PduInfoPtr->SduLength < 1U)
    {
        return;
    }

    SequenceNumber = PduInfoPtr->SduDataPtr[0] & CANTP_PCI_SN_MASK;

    if (SequenceNumber != CanTp_RxRuntime.ExpectedSequenceNumber)
    {
        CANTP_DEBUG_PRINTF(
            "[CanTp][RX-ERR] SN mismatch. expected=%u, received=%u, receivedLength=%u, totalLength=%u\r\n",
            (unsigned int)CanTp_RxRuntime.ExpectedSequenceNumber,
            (unsigned int)SequenceNumber,
            (unsigned int)CanTp_RxRuntime.ReceivedLength,
            (unsigned int)CanTp_RxRuntime.TotalLength
        );

        CanTp_ResetRxRuntime();
        return;
    }

    RemainingLength =
        CanTp_RxRuntime.TotalLength - CanTp_RxRuntime.ReceivedLength;

    if (RemainingLength > CANTP_CF_DATA_LENGTH)
    {
        CopyLength = CANTP_CF_DATA_LENGTH;
    }
    else
    {
        CopyLength = (uint8)RemainingLength;
    }

    if ((CanTp_RxRuntime.ReceivedLength + CopyLength) > CanTp_RxRuntime.TotalLength)
    {
        CanTp_ResetRxRuntime();
        return;
    }

    if ((CanTp_RxRuntime.ReceivedLength + CopyLength) > RxConfig->RxBufferSize)
    {
        CanTp_ResetRxRuntime();
        return;
    }

    memcpy(
        &CanTp_RxRuntime.Buffer[CanTp_RxRuntime.ReceivedLength],
        &PduInfoPtr->SduDataPtr[1],
        CopyLength
    );

    CanTp_RxRuntime.ReceivedLength += CopyLength;

    CanTp_RxRuntime.ExpectedSequenceNumber =
        (uint8)((CanTp_RxRuntime.ExpectedSequenceNumber + 1U) & CANTP_PCI_SN_MASK);

    CanTp_RxRuntime.BlockCounter++;

    if (CanTp_RxRuntime.ReceivedLength >= CanTp_RxRuntime.TotalLength)
    {
        CanTpRxInfo.SduDataPtr = CanTp_RxRuntime.Buffer;
        CanTpRxInfo.SduLength  = CanTp_RxRuntime.TotalLength;

        CANTP_DEBUG_LOG_PACKET(
            "RX-COMPLETE",
            CanTp_RxRuntime.CanTpRxNsduId,
            &CanTpRxInfo
        );

        PduR_CanTpRxIndication(
            CanTp_RxRuntime.PduRRxPduId,
            &CanTpRxInfo
        );

        CanTp_ResetRxRuntime();
        return;
    }

    if ((RxConfig->BlockSize != 0U) &&
        (CanTp_RxRuntime.BlockCounter >= RxConfig->BlockSize))
    {
        CanTp_RxRuntime.BlockCounter = 0U;

        (void)CanTp_SendFlowControl(
            RxConfig,
            CANTP_FC_STATUS_CTS
        );
    }
}

static Std_ReturnType CanTp_SendFlowControl(
    const CanTp_RxNsduConfigType* RxConfig,
    uint8 FlowStatus
)
{
    PduInfoType CanIfPduInfo;

    memset(CanTp_RxRuntime.FcFrameBuffer, 0x00U, CANTP_CAN_FRAME_LENGTH);

    CanTp_RxRuntime.FcFrameBuffer[0] =
        (uint8)(CANTP_PCI_TYPE_FC |
        (FlowStatus & CANTP_PCI_LENGTH_MASK));

    CanTp_RxRuntime.FcFrameBuffer[1] = RxConfig->BlockSize;
    CanTp_RxRuntime.FcFrameBuffer[2] = RxConfig->STmin;

    CanIfPduInfo.SduDataPtr = CanTp_RxRuntime.FcFrameBuffer;
    CanIfPduInfo.SduLength  = CANTP_CAN_FRAME_LENGTH;

    CANTP_DEBUG_LOG_PACKET(
        "TX",
        RxConfig->CanIfTxFcPduId,
        &CanIfPduInfo
    );

    return CanIf_Transmit(
        RxConfig->CanIfTxFcPduId,
        &CanIfPduInfo
    );
}

static void CanTp_HandleFlowControl(
    const PduInfoType* PduInfoPtr
)
{
    uint8 FlowStatus;
    uint8 BlockSize;
    uint8 STmin;

    if (PduInfoPtr->SduLength < 3U)
    {
        return;
    }

    if (CanTp_TxRuntime.State != CANTP_TX_STATE_WAIT_FC)
    {
        return;
    }

    FlowStatus = PduInfoPtr->SduDataPtr[0] & CANTP_PCI_LENGTH_MASK;
    BlockSize  = PduInfoPtr->SduDataPtr[1];
    STmin      = PduInfoPtr->SduDataPtr[2];

    if (FlowStatus == CANTP_FC_STATUS_CTS)
    {
        CanTp_TxRuntime.BlockSize      = BlockSize;
        CanTp_TxRuntime.BlockCounter   = 0U;
        CanTp_TxRuntime.STmin          = STmin;
        CanTp_TxRuntime.WaitFrameCount = 0U;
        CanTp_TxRuntime.State          = CANTP_TX_STATE_SEND_CF;
    }
    else if (FlowStatus == CANTP_FC_STATUS_WAIT)
    {
        CanTp_TxRuntime.WaitFrameCount++;

        if (CanTp_TxRuntime.WaitFrameCount > CANTP_MAX_WAIT_FRAME_COUNT)
        {
            PduR_CanTpTxConfirmation(
                CanTp_TxRuntime.PduRTxPduId,
                E_NOT_OK
            );

            CanTp_ResetTxRuntime();
        }
        else
        {
            CanTp_TxRuntime.State = CANTP_TX_STATE_WAIT_FC;
        }
    }
    else
    {
        PduR_CanTpTxConfirmation(
            CanTp_TxRuntime.PduRTxPduId,
            E_NOT_OK
        );

        CanTp_ResetTxRuntime();
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

static void CanTp_ResetTxRuntime(void)
{
    memset(&CanTp_TxRuntime, 0x00U, sizeof(CanTp_TxRuntime));
    CanTp_TxRuntime.State = CANTP_TX_STATE_IDLE;
}

static void CanTp_ResetRxRuntime(void)
{
    memset(&CanTp_RxRuntime, 0x00U, sizeof(CanTp_RxRuntime));
    CanTp_RxRuntime.State = CANTP_RX_STATE_IDLE;
}
