/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"

#include "DoIP.h"
#include "DoIP_Cfg.h"

#include "SoAd.h"
#include "PduR.h"

#include "UART.h"

#include <string.h>

/*********************************************************************************************************************/
/*------------------------------------------------------Types--------------------------------------------------------*/
/*********************************************************************************************************************/

typedef enum
{
    DOIP_STATE_INITIALIZED = 0U,
    DOIP_STATE_ROUTING_ACTIVE
} DoIP_StateType;

typedef struct
{
    DoIP_StateType State;

    uint16 TesterLogicalAddress;
    uint16 EntityLogicalAddress;

    uint8 RxBuffer[DOIP_RX_BUFFER_SIZE];
    uint16 RxLength;

    uint8 TxBuffer[DOIP_TX_BUFFER_SIZE];
} DoIP_RuntimeType;

/*********************************************************************************************************************/
/*------------------------------------------------Static Variables---------------------------------------------------*/
/*********************************************************************************************************************/

static DoIP_RuntimeType DoIP_Runtime;

/*********************************************************************************************************************/
/*------------------------------------------------Private Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static void DoIP_ProcessRxBuffer(void);

static void DoIP_HandleMessage(
    uint16 PayloadType,
    const uint8* PayloadPtr,
    uint32 PayloadLength
);

static void DoIP_HandleRoutingActivation(
    const uint8* PayloadPtr,
    uint32 PayloadLength
);

static void DoIP_HandleDiagnosticMessage(
    const uint8* PayloadPtr,
    uint32 PayloadLength
);

static Std_ReturnType DoIP_SendMessage(
    uint16 PayloadType,
    const uint8* PayloadPtr,
    uint32 PayloadLength
);

static Std_ReturnType DoIP_SendGenericNack(
    uint8 NackCode
);

static Std_ReturnType DoIP_SendRoutingActivationResponse(
    uint8 ResponseCode
);

static Std_ReturnType DoIP_SendDiagnosticPositiveAck(
    uint16 RequestSourceAddress,
    uint16 RequestTargetAddress
);

static Std_ReturnType DoIP_SendDiagnosticNegativeAck(
    uint16 RequestSourceAddress,
    uint16 RequestTargetAddress,
    uint8 NackCode
);

static const DoIP_RxPduConfigType* DoIP_FindRxPduConfigByTargetAddress(
    uint16 TargetAddress
);

static const DoIP_TxPduConfigType* DoIP_FindTxPduConfig(
    PduIdType DoIPTxPduId
);

static uint16 DoIP_ParseUint16BigEndian(
    const uint8* DataPtr
);

static uint32 DoIP_ParseUint32BigEndian(
    const uint8* DataPtr
);

static void DoIP_WriteUint16BigEndian(
    uint8* DataPtr,
    uint16 Value
);

static void DoIP_WriteUint32BigEndian(
    uint8* DataPtr,
    uint32 Value
);

/*********************************************************************************************************************/
/*------------------------------------------------Public Functions---------------------------------------------------*/
/*********************************************************************************************************************/

void DoIP_Init(void)
{
    memset(
        &DoIP_Runtime,
        0x00,
        sizeof(DoIP_Runtime)
    );

    DoIP_Runtime.State = DOIP_STATE_INITIALIZED;
    DoIP_Runtime.TesterLogicalAddress = DoIP_Config.TesterLogicalAddressDefault;
    DoIP_Runtime.EntityLogicalAddress = DoIP_Config.EntityLogicalAddress;
    DoIP_Runtime.RxLength = 0U;
}

void DoIP_TpRxIndication(
    PduIdType SoAdRxPduId,
    const PduInfoType* PduInfoPtr
)
{
    if (SoAdRxPduId != DoIP_Config.SoAdRxPduId)
    {
        return;
    }

    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR))
    {
        return;
    }

    if ((DoIP_Runtime.RxLength + PduInfoPtr->SduLength) > DOIP_RX_BUFFER_SIZE)
    {
        (void)DoIP_SendGenericNack(
            DOIP_GENERIC_NACK_MESSAGE_TOO_LARGE
        );

        DoIP_Runtime.RxLength = 0U;
        return;
    }

    memcpy(
        &DoIP_Runtime.RxBuffer[DoIP_Runtime.RxLength],
        PduInfoPtr->SduDataPtr,
        PduInfoPtr->SduLength
    );

    DoIP_Runtime.RxLength =
        (uint16)(DoIP_Runtime.RxLength + PduInfoPtr->SduLength);

    DoIP_ProcessRxBuffer();
}

Std_ReturnType DoIP_TpTransmit(
    PduIdType DoIPTxPduId,
    const PduInfoType* PduInfoPtr
)
{
    const DoIP_TxPduConfigType* TxConfig;
    uint8* PayloadPtr;
    uint32 PayloadLength;

    TxConfig = DoIP_FindTxPduConfig(
        DoIPTxPduId
    );

    if (TxConfig == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (DoIP_Runtime.State != DOIP_STATE_ROUTING_ACTIVE)
    {
        return E_NOT_OK;
    }

    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    PayloadLength = (uint32)PduInfoPtr->SduLength + 4U;

    if ((PayloadLength + DOIP_HEADER_LENGTH) > DOIP_TX_BUFFER_SIZE)
    {
        return E_NOT_OK;
    }

    PayloadPtr = &DoIP_Runtime.TxBuffer[DOIP_HEADER_LENGTH];

    /*
     * PduR은 DoIPTxPduId만 전달한다.
     * DoIP는 DoIPTxPduId를 기준으로 Source Logical Address를 결정한다.
     *
     * 현재 프로젝트에서는 CAN Target ECU 응답만 존재하므로
     * SourceAddress는 DOIP_LOGICAL_ADDRESS_TARGET_ECU가 된다.
     */
    DoIP_WriteUint16BigEndian(
        &PayloadPtr[0],
        TxConfig->SourceAddress
    );

    /*
     * 응답의 TargetAddress는 마지막 Diagnostic Request를 보낸 Tester Logical Address이다.
     */
    DoIP_WriteUint16BigEndian(
        &PayloadPtr[2],
        DoIP_Runtime.TesterLogicalAddress
    );

    memcpy(
        &PayloadPtr[4],
        PduInfoPtr->SduDataPtr,
        PduInfoPtr->SduLength
    );

    return DoIP_SendMessage(
        DOIP_PAYLOAD_TYPE_DIAG_MESSAGE,
        PayloadPtr,
        PayloadLength
    );
}

/*********************************************************************************************************************/
/*------------------------------------------------DoIP Message Processing--------------------------------------------*/
/*********************************************************************************************************************/

static void DoIP_ProcessRxBuffer(void)
{
    uint16 Offset;
    uint16 RemainingLength;
    uint16 PayloadType;
    uint32 PayloadLength;
    uint32 MessageLength;
    uint8 ProtocolVersion;
    uint8 InverseProtocolVersion;

    Offset = 0U;

    while ((DoIP_Runtime.RxLength - Offset) >= DOIP_HEADER_LENGTH)
    {
        ProtocolVersion = DoIP_Runtime.RxBuffer[Offset + 0U];
        InverseProtocolVersion = DoIP_Runtime.RxBuffer[Offset + 1U];

        if ((ProtocolVersion != DOIP_PROTOCOL_VERSION) ||
            (InverseProtocolVersion != DOIP_INVERSE_PROTOCOL_VERSION))
        {
            (void)DoIP_SendGenericNack(
                DOIP_GENERIC_NACK_INVALID_HEADER
            );

            DoIP_Runtime.RxLength = 0U;
            return;
        }

        PayloadType = DoIP_ParseUint16BigEndian(
            &DoIP_Runtime.RxBuffer[Offset + 2U]
        );

        PayloadLength = DoIP_ParseUint32BigEndian(
            &DoIP_Runtime.RxBuffer[Offset + 4U]
        );

        MessageLength = DOIP_HEADER_LENGTH + PayloadLength;

        if (MessageLength > DOIP_RX_BUFFER_SIZE)
        {
            (void)DoIP_SendGenericNack(
                DOIP_GENERIC_NACK_MESSAGE_TOO_LARGE
            );

            DoIP_Runtime.RxLength = 0U;
            return;
        }

        /*
         * TCP는 byte-stream이므로 DoIP 메시지가 분할 수신될 수 있다.
         * 아직 하나의 DoIP 메시지 전체가 도착하지 않았다면 다음 수신을 기다린다.
         */
        if ((DoIP_Runtime.RxLength - Offset) < MessageLength)
        {
            break;
        }

        DoIP_HandleMessage(
            PayloadType,
            &DoIP_Runtime.RxBuffer[Offset + DOIP_HEADER_LENGTH],
            PayloadLength
        );

        Offset = (uint16)(Offset + MessageLength);
    }

    if (Offset > 0U)
    {
        RemainingLength = (uint16)(DoIP_Runtime.RxLength - Offset);

        if (RemainingLength > 0U)
        {
            memmove(
                DoIP_Runtime.RxBuffer,
                &DoIP_Runtime.RxBuffer[Offset],
                RemainingLength
            );
        }

        DoIP_Runtime.RxLength = RemainingLength;
    }
}

static void DoIP_HandleMessage(
    uint16 PayloadType,
    const uint8* PayloadPtr,
    uint32 PayloadLength
)
{
    switch (PayloadType)
    {
        case DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQ:
        {
            DoIP_HandleRoutingActivation(
                PayloadPtr,
                PayloadLength
            );
            break;
        }

        case DOIP_PAYLOAD_TYPE_DIAG_MESSAGE:
        {
            DoIP_HandleDiagnosticMessage(
                PayloadPtr,
                PayloadLength
            );
            break;
        }

        default:
        {
            (void)DoIP_SendGenericNack(
                DOIP_GENERIC_NACK_UNKNOWN_PAYLOAD_TYPE
            );
            break;
        }
    }
}

static void DoIP_HandleRoutingActivation(
    const uint8* PayloadPtr,
    uint32 PayloadLength
)
{
    if (PayloadLength < 3U)
    {
        (void)DoIP_SendGenericNack(
            DOIP_GENERIC_NACK_INVALID_HEADER
        );
        return;
    }

    DoIP_Runtime.TesterLogicalAddress = DoIP_ParseUint16BigEndian(
        &PayloadPtr[0]
    );

    DoIP_Runtime.State = DOIP_STATE_ROUTING_ACTIVE;

    (void)DoIP_SendRoutingActivationResponse(
        DOIP_ROUTING_ACTIVATION_RES_SUCCESS
    );
}

static void DoIP_HandleDiagnosticMessage(
    const uint8* PayloadPtr,
    uint32 PayloadLength
)
{
    const DoIP_RxPduConfigType* RxConfig;
    uint16 SourceAddress;
    uint16 TargetAddress;
    PduInfoType UdsPduInfo;

    /*
     * DoIP Diagnostic Message Payload 구조:
     *
     * Byte 0~1 : SourceAddress
     * Byte 2~3 : TargetAddress
     * Byte 4~  : UDS Payload
     */
    if (PayloadLength < 5U)
    {
        (void)DoIP_SendGenericNack(
            DOIP_GENERIC_NACK_INVALID_HEADER
        );
        return;
    }

    if (DoIP_Runtime.State != DOIP_STATE_ROUTING_ACTIVE)
    {
        return;
    }

    SourceAddress = DoIP_ParseUint16BigEndian(
        &PayloadPtr[0]
    );

    TargetAddress = DoIP_ParseUint16BigEndian(
        &PayloadPtr[2]
    );

    /*
     * 현재 프로젝트에서는 Gateway 자체 진단을 지원하지 않는다.
     *
     * 따라서 TargetAddress가 Gateway ECU 주소인 경우에도 Local DCM으로 보내지 않는다.
     * 오직 DoIP_RxPduConfig에 등록된 Target ECU 주소만 지원한다.
     */
    RxConfig = DoIP_FindRxPduConfigByTargetAddress(
        TargetAddress
    );

    if (RxConfig == NULL_PTR)
    {
        (void)DoIP_SendDiagnosticNegativeAck(
            SourceAddress,
            TargetAddress,
            DOIP_DIAG_NACK_CODE_UNKNOWN_TARGET
        );

        return;
    }

    DoIP_Runtime.TesterLogicalAddress = SourceAddress;

    UdsPduInfo.SduDataPtr = (uint8*)&PayloadPtr[4];
    UdsPduInfo.SduLength  = (PduLengthType)(PayloadLength - 4U);

    /*
     * DoIP 계층에서 Diagnostic Message 수신 자체는 정상 처리되었으므로
     * Diagnostic Positive ACK를 먼저 전송한다.
     */
    (void)DoIP_SendDiagnosticPositiveAck(
        SourceAddress,
        TargetAddress
    );

    /*
     * DoIP는 Logical Address를 PDU ID로 변환한다.
     * PduR은 Logical Address를 직접 해석하지 않고 DoIPRxPduId만 보고 라우팅한다.
     *
     * 현재 구조:
     * TargetAddress = DOIP_LOGICAL_ADDRESS_TARGET_ECU
     * → DoIPRxPduId = DOIP_RXPDU_DIAG_REQ_TO_CANTP
     * → PduR route  = DoIP → CanTp
     */
    PduR_DoIPTpRxIndication(
        RxConfig->DoIPRxPduId,
        &UdsPduInfo
    );
}

/*********************************************************************************************************************/
/*------------------------------------------------DoIP Tx Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static Std_ReturnType DoIP_SendMessage(
    uint16 PayloadType,
    const uint8* PayloadPtr,
    uint32 PayloadLength
)
{
    PduInfoType SoAdPduInfo;
    uint32 TotalLength;

    TotalLength = DOIP_HEADER_LENGTH + PayloadLength;

    if (TotalLength > DOIP_TX_BUFFER_SIZE)
    {
        return E_NOT_OK;
    }

    DoIP_Runtime.TxBuffer[0] = DOIP_PROTOCOL_VERSION;
    DoIP_Runtime.TxBuffer[1] = DOIP_INVERSE_PROTOCOL_VERSION;

    DoIP_WriteUint16BigEndian(
        &DoIP_Runtime.TxBuffer[2],
        PayloadType
    );

    DoIP_WriteUint32BigEndian(
        &DoIP_Runtime.TxBuffer[4],
        PayloadLength
    );

    if ((PayloadPtr != NULL_PTR) && (PayloadLength > 0U))
    {
        memcpy(
            &DoIP_Runtime.TxBuffer[DOIP_HEADER_LENGTH],
            PayloadPtr,
            PayloadLength
        );
    }

    SoAdPduInfo.SduDataPtr = DoIP_Runtime.TxBuffer;
    SoAdPduInfo.SduLength  = (PduLengthType)TotalLength;

    return SoAd_Transmit(
        DoIP_Config.SoAdTxPduId,
        &SoAdPduInfo
    );
}

static Std_ReturnType DoIP_SendGenericNack(
    uint8 NackCode
)
{
    uint8 Payload[1];

    Payload[0] = NackCode;

    return DoIP_SendMessage(
        DOIP_PAYLOAD_TYPE_GENERIC_NACK,
        Payload,
        1U
    );
}

static Std_ReturnType DoIP_SendRoutingActivationResponse(
    uint8 ResponseCode
)
{
    uint8 Payload[9];

    /*
     * Routing Activation Response Payload:
     *
     * Byte 0~1 : Tester Logical Address
     * Byte 2~3 : Entity Logical Address
     * Byte 4   : Response Code
     * Byte 5~8 : Reserved / OEM specific
     */
    DoIP_WriteUint16BigEndian(
        &Payload[0],
        DoIP_Runtime.TesterLogicalAddress
    );

    DoIP_WriteUint16BigEndian(
        &Payload[2],
        DoIP_Runtime.EntityLogicalAddress
    );

    Payload[4] = ResponseCode;

    Payload[5] = 0x00U;
    Payload[6] = 0x00U;
    Payload[7] = 0x00U;
    Payload[8] = 0x00U;

    return DoIP_SendMessage(
        DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RES,
        Payload,
        9U
    );
}

static Std_ReturnType DoIP_SendDiagnosticPositiveAck(
    uint16 RequestSourceAddress,
    uint16 RequestTargetAddress
)
{
    uint8 Payload[5];

    /*
     * Diagnostic Positive ACK는 수신한 Diagnostic Message에 대한 DoIP 계층 ACK이다.
     *
     * Request:
     *   SourceAddress = Tester
     *   TargetAddress = Target ECU
     *
     * ACK:
     *   SourceAddress = Target ECU
     *   TargetAddress = Tester
     */
    DoIP_WriteUint16BigEndian(
        &Payload[0],
        RequestTargetAddress
    );

    DoIP_WriteUint16BigEndian(
        &Payload[2],
        RequestSourceAddress
    );

    Payload[4] = DOIP_DIAG_ACK_CODE_OK;

    return DoIP_SendMessage(
        DOIP_PAYLOAD_TYPE_DIAG_POS_ACK,
        Payload,
        5U
    );
}

static Std_ReturnType DoIP_SendDiagnosticNegativeAck(
    uint16 RequestSourceAddress,
    uint16 RequestTargetAddress,
    uint8 NackCode
)
{
    uint8 Payload[5];

    /*
     * Diagnostic Negative ACK도 요청 방향의 반대 방향으로 구성한다.
     *
     * Unknown Target인 경우에도 수신한 TargetAddress를 SourceAddress 위치에 넣어
     * 어떤 Target에 대한 거절인지 표현한다.
     */
    DoIP_WriteUint16BigEndian(
        &Payload[0],
        RequestTargetAddress
    );

    DoIP_WriteUint16BigEndian(
        &Payload[2],
        RequestSourceAddress
    );

    Payload[4] = NackCode;

    return DoIP_SendMessage(
        DOIP_PAYLOAD_TYPE_DIAG_NEG_ACK,
        Payload,
        5U
    );
}

/*********************************************************************************************************************/
/*------------------------------------------------Config Lookup Functions--------------------------------------------*/
/*********************************************************************************************************************/

static const DoIP_RxPduConfigType* DoIP_FindRxPduConfigByTargetAddress(
    uint16 TargetAddress
)
{
    uint8 Index;

    for (Index = 0U; Index < DOIP_RXPDU_COUNT; Index++)
    {
        if (DoIP_RxPduConfig[Index].TargetAddress == TargetAddress)
        {
            return &DoIP_RxPduConfig[Index];
        }
    }

    return NULL_PTR;
}

static const DoIP_TxPduConfigType* DoIP_FindTxPduConfig(
    PduIdType DoIPTxPduId
)
{
    uint8 Index;

    for (Index = 0U; Index < DOIP_TXPDU_COUNT; Index++)
    {
        if (DoIP_TxPduConfig[Index].DoIPTxPduId == DoIPTxPduId)
        {
            return &DoIP_TxPduConfig[Index];
        }
    }

    return NULL_PTR;
}

/*********************************************************************************************************************/
/*------------------------------------------------Utility Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static uint16 DoIP_ParseUint16BigEndian(
    const uint8* DataPtr
)
{
    return (uint16)(((uint16)DataPtr[0] << 8U) |
                    ((uint16)DataPtr[1]));
}

static uint32 DoIP_ParseUint32BigEndian(
    const uint8* DataPtr
)
{
    return (((uint32)DataPtr[0] << 24U) |
            ((uint32)DataPtr[1] << 16U) |
            ((uint32)DataPtr[2] << 8U)  |
            ((uint32)DataPtr[3]));
}

static void DoIP_WriteUint16BigEndian(
    uint8* DataPtr,
    uint16 Value
)
{
    DataPtr[0] = (uint8)((Value >> 8U) & 0xFFU);
    DataPtr[1] = (uint8)(Value & 0xFFU);
}

static void DoIP_WriteUint32BigEndian(
    uint8* DataPtr,
    uint32 Value
)
{
    DataPtr[0] = (uint8)((Value >> 24U) & 0xFFU);
    DataPtr[1] = (uint8)((Value >> 16U) & 0xFFU);
    DataPtr[2] = (uint8)((Value >> 8U) & 0xFFU);
    DataPtr[3] = (uint8)(Value & 0xFFU);
}