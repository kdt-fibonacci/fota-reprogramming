/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "Dcm.h"
#include "Dcm_cfg.h"

#include "PduR.h"

#include "Debug_Log.h"

#include <string.h>

/*********************************************************************************************************************/
/*------------------------------------------------------Types--------------------------------------------------------*/
/*********************************************************************************************************************/

typedef enum
{
    DCM_REQUEST_IDLE = 0U,
    DCM_REQUEST_PENDING
} Dcm_RequestStateType;

typedef struct
{
    Dcm_SessionType CurrentSession;

    Dcm_FotaStateType FotaState;
    Dcm_FotaResultType LastFotaResult;

    Dcm_RequestStateType RequestState;
    boolean FotaReportPending;
    boolean FotaIdleTransitionPending;

    PduIdType CurrentRxPduId;

    uint8 RxBuffer[DCM_RX_BUFFER_SIZE];
    PduLengthType RxLength;

    uint8 TxBuffer[DCM_TX_BUFFER_SIZE];
    PduLengthType TxLength;

    uint8 ExpectedBlockSequenceCounter;
} Dcm_RuntimeType;

/*********************************************************************************************************************/
/*------------------------------------------------Static Variables---------------------------------------------------*/
/*********************************************************************************************************************/

static Dcm_RuntimeType Dcm_Runtime;

/*********************************************************************************************************************/
/*------------------------------------------------Private Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static void Dcm_ProcessRequest(void);

static void Dcm_DispatchService(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
);

static void Dcm_HandleDiagnosticSessionControl(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
);

static void Dcm_HandleEcuReset(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
);

static void Dcm_HandleTesterPresent(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
);

static void Dcm_HandleRequestDownload(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
);

static void Dcm_HandleTransferData(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
);

static void Dcm_HandleRequestTransferExit(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
);

static void Dcm_HandleRoutineControl(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
);

static void Dcm_HandleReadDataByIdentifier(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
);

static void Dcm_SendPositiveResponse(
    uint8 Sid,
    const uint8* PayloadPtr,
    PduLengthType PayloadLength
);

static void Dcm_SendNegativeResponse(
    uint8 Sid,
    uint8 Nrc
);

static Std_ReturnType Dcm_SendResponse(
    const uint8* ResponseDataPtr,
    PduLengthType ResponseLength
);

static uint16 Dcm_ParseUint16BigEndian(
    const uint8* DataPtr
);

static void Dcm_WriteUint16BigEndian(
    uint8* DataPtr,
    uint16 Value
);

static void Dcm_WriteUint24BigEndian(
    uint8* DataPtr,
    uint32 Value
);

static void Dcm_ResetFotaDownloadContext(void);

static boolean Dcm_IsFotaDownloadStartState(
    Dcm_FotaStateType FotaState
);

/*********************************************************************************************************************/
/*------------------------------------------------Public Functions---------------------------------------------------*/
/*********************************************************************************************************************/

void Dcm_Init(void)
{
    memset(
        &Dcm_Runtime,
        0x00,
        sizeof(Dcm_Runtime)
    );

    Dcm_Runtime.CurrentSession = DCM_SESSION_DEFAULT;

    Dcm_Runtime.FotaState = DCM_FOTA_STATE_IDLE;
    Dcm_Runtime.LastFotaResult = DCM_FOTA_RESULT_NONE;

    Dcm_Runtime.RequestState = DCM_REQUEST_IDLE;
    Dcm_Runtime.CurrentRxPduId = DCM_RXPDU_DIAG_REQ;

    Dcm_Runtime.ExpectedBlockSequenceCounter = 1U;
}

void Dcm_RxIndication(
    PduIdType DcmRxPduId,
    const PduInfoType* PduInfoPtr
)
{
    if (DcmRxPduId != DCM_RXPDU_DIAG_REQ)
    {
        return;
    }

    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR))
    {
        return;
    }

    if (PduInfoPtr->SduLength > DCM_RX_BUFFER_SIZE)
    {
        return;
    }

    DCM_DEBUG_PRINTF(
        "[Dcm][RX] DcmRxPduId=%u",
        (unsigned int)DcmRxPduId
    );
    DCM_DEBUG_PRINT_PDU("", PduInfoPtr);

    /*
     * Dcm_RxIndication에서는 요청을 복사만 하고,
     * 실제 UDS 처리는 Dcm_MainFunction에서 수행한다.
     *
     * 이렇게 하면 CanTp/PduR 콜백 안에서 서비스 처리를 길게 수행하지 않아도 된다.
     */
    memcpy(
        Dcm_Runtime.RxBuffer,
        PduInfoPtr->SduDataPtr,
        PduInfoPtr->SduLength
    );

    Dcm_Runtime.RxLength = PduInfoPtr->SduLength;
    Dcm_Runtime.CurrentRxPduId = DcmRxPduId;
    Dcm_Runtime.RequestState = DCM_REQUEST_PENDING;
}

void Dcm_TxConfirmation(
    PduIdType DcmTxPduId,
    Std_ReturnType Result
)
{
    Dcm_FotaStateType ReportedFotaState;

    if (DcmTxPduId != DCM_TXPDU_DIAG_RES)
    {
        return;
    }

    DCM_DEBUG_PRINTF(
        "[Dcm][TX-CNF] DcmTxPduId=%u Result=%u\r\n",
        (unsigned int)DcmTxPduId,
        (unsigned int)Result
    );

    /*
     * 현재는 송신 완료 결과를 상태 전이에 크게 사용하지 않는다.
     * 추후 Response pending, session timeout, FOTA result report 등에 연결 가능하다.
     */
    if (Result != E_OK)
    {
        Dcm_Runtime.LastFotaResult = DCM_FOTA_RESULT_FAILED;

        if ((Dcm_Runtime.FotaState != DCM_FOTA_STATE_IDLE) &&
            (Dcm_Runtime.FotaState != DCM_FOTA_STATE_ACTIVATED))
        {
            Dcm_Runtime.FotaState = DCM_FOTA_STATE_FAILED;
        }
    }
    else if (Dcm_Runtime.FotaIdleTransitionPending == TRUE)
    {
        ReportedFotaState = Dcm_Runtime.FotaState;

        Dcm_Runtime.FotaIdleTransitionPending = FALSE;
        Dcm_Runtime.FotaState = DCM_FOTA_STATE_IDLE;
        Dcm_ResetFotaDownloadContext();

        if (ReportedFotaState == DCM_FOTA_STATE_ROLLBACK_DONE)
        {
            DCM_DEBUG_PRINTF("[Dcm][FOTA] ROLLBACK_DONE reported. FotaState -> IDLE, context reset\r\n");
        }
        else
        {
            DCM_DEBUG_PRINTF("[Dcm][FOTA] ACTIVATED reported. FotaState -> IDLE, context reset\r\n");
        }
    }
}

void Dcm_MainFunction(void)
{
    if (Dcm_Runtime.RequestState == DCM_REQUEST_PENDING)
    {
        Dcm_ProcessRequest();
        Dcm_Runtime.RequestState = DCM_REQUEST_IDLE;
    }
}

Dcm_SessionType Dcm_GetCurrentSession(void)
{
    return Dcm_Runtime.CurrentSession;
}

Dcm_FotaStateType Dcm_GetFotaState(void)
{
    return Dcm_Runtime.FotaState;
}

Dcm_FotaResultType Dcm_GetLastFotaResult(void)
{
    return Dcm_Runtime.LastFotaResult;
}

/*********************************************************************************************************************/
/*------------------------------------------------Request Processing-------------------------------------------------*/
/*********************************************************************************************************************/

static void Dcm_ProcessRequest(void)
{
    if (Dcm_Runtime.RxLength == 0U)
    {
        return;
    }

    Dcm_DispatchService(
        Dcm_Runtime.RxBuffer,
        Dcm_Runtime.RxLength
    );
}

static void Dcm_DispatchService(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
)
{
    uint8 Sid;

    if ((RequestDataPtr == NULL_PTR) || (RequestLength == 0U))
    {
        return;
    }

    Sid = RequestDataPtr[0];

    switch (Sid)
    {
        case DCM_SID_DIAGNOSTIC_SESSION_CONTROL:
        {
            Dcm_HandleDiagnosticSessionControl(
                RequestDataPtr,
                RequestLength
            );
            break;
        }

        case DCM_SID_ECU_RESET:
        {
            Dcm_HandleEcuReset(
                RequestDataPtr,
                RequestLength
            );
            break;
        }

        case DCM_SID_TESTER_PRESENT:
        {
            Dcm_HandleTesterPresent(
                RequestDataPtr,
                RequestLength
            );
            break;
        }

        case DCM_SID_REQUEST_DOWNLOAD:
        {
            Dcm_HandleRequestDownload(
                RequestDataPtr,
                RequestLength
            );
            break;
        }

        case DCM_SID_TRANSFER_DATA:
        {
            Dcm_HandleTransferData(
                RequestDataPtr,
                RequestLength
            );
            break;
        }

        case DCM_SID_REQUEST_TRANSFER_EXIT:
        {
            Dcm_HandleRequestTransferExit(
                RequestDataPtr,
                RequestLength
            );
            break;
        }

        case DCM_SID_ROUTINE_CONTROL:
        {
            Dcm_HandleRoutineControl(
                RequestDataPtr,
                RequestLength
            );
            break;
        }

        case DCM_SID_READ_DATA_BY_IDENTIFIER:
        {
            Dcm_HandleReadDataByIdentifier(
                RequestDataPtr,
                RequestLength
            );
            break;
        }

        default:
        {
            Dcm_SendNegativeResponse(
                Sid,
                DCM_NRC_SERVICE_NOT_SUPPORTED
            );
            break;
        }
    }
}

/*********************************************************************************************************************/
/*------------------------------------------------UDS Service Handlers-----------------------------------------------*/
/*********************************************************************************************************************/

static void Dcm_HandleDiagnosticSessionControl(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
)
{
    uint8 SubFunction;
    uint8 ResponsePayload[5];

    if (RequestLength < 2U)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_DIAGNOSTIC_SESSION_CONTROL,
            DCM_NRC_INCORRECT_MESSAGE_LENGTH
        );
        return;
    }

    SubFunction = RequestDataPtr[1] & 0x7FU;

    switch (SubFunction)
    {
        case DCM_SESSION_VALUE_DEFAULT:
        {
            Dcm_Runtime.CurrentSession = DCM_SESSION_DEFAULT;
            break;
        }

        case DCM_SESSION_VALUE_EXTENDED:
        {
            Dcm_Runtime.CurrentSession = DCM_SESSION_EXTENDED;

            if (Dcm_IsFotaDownloadStartState(Dcm_Runtime.FotaState) == TRUE)
            {
                Dcm_Runtime.FotaState = DCM_FOTA_STATE_EXTENDED_SESSION;
            }
            break;
        }

        case DCM_SESSION_VALUE_PROGRAMMING:
        {
            /*
             * Programming Session은 다운로드/검증 흐름 이후 진입하도록 제한한다.
             * 프로젝트 단순화를 위해 EXTENDED_SESSION 이후에도 허용 가능하지만,
             * 여기서는 FOTA 흐름을 보이기 위해 VERIFIED 이후를 기본 조건으로 둔다.
             */
            if ((Dcm_Runtime.FotaState != DCM_FOTA_STATE_VERIFIED) &&
                (Dcm_Runtime.FotaState != DCM_FOTA_STATE_PROGRAMMING_SESSION))
            {
                Dcm_SendNegativeResponse(
                    DCM_SID_DIAGNOSTIC_SESSION_CONTROL,
                    DCM_NRC_CONDITIONS_NOT_CORRECT
                );
                return;
            }

            Dcm_Runtime.CurrentSession = DCM_SESSION_PROGRAMMING;
            Dcm_Runtime.FotaState = DCM_FOTA_STATE_PROGRAMMING_SESSION;
            break;
        }

        default:
        {
            Dcm_SendNegativeResponse(
                DCM_SID_DIAGNOSTIC_SESSION_CONTROL,
                DCM_NRC_SUBFUNCTION_NOT_SUPPORTED
            );
            return;
        }
    }

    /*
     * Positive Response:
     * 0x50, sessionType, P2ServerMax high, P2ServerMax low, P2*ServerMax high, P2*ServerMax low
     *
     * 여기서는 테스트용 timing 값을 임의로 고정한다.
     */
    ResponsePayload[0] = SubFunction;
    ResponsePayload[1] = 0x00U;
    ResponsePayload[2] = 0x32U;
    ResponsePayload[3] = 0x01U;
    ResponsePayload[4] = 0xF4U;

    Dcm_SendPositiveResponse(
        DCM_SID_DIAGNOSTIC_SESSION_CONTROL,
        ResponsePayload,
        5U
    );
}

static void Dcm_HandleEcuReset(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
)
{
    uint8 ResetType;
    uint8 ResponsePayload[1];

    if (RequestLength < 2U)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_ECU_RESET,
            DCM_NRC_INCORRECT_MESSAGE_LENGTH
        );
        return;
    }

    ResetType = RequestDataPtr[1] & 0x7FU;

    if (ResetType != DCM_RESET_HARD_RESET)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_ECU_RESET,
            DCM_NRC_SUBFUNCTION_NOT_SUPPORTED
        );
        return;
    }

    /*
     * 실제 reset 수행은 응답 송신 이후 별도 플래그로 처리하는 것이 안전하다.
     * 현재는 상태만 Activation Pending/Activated로 갱신한다.
     */
    if (Dcm_Runtime.FotaState == DCM_FOTA_STATE_ACTIVATION_PENDING)
    {
        Dcm_Runtime.FotaState = DCM_FOTA_STATE_ACTIVATED;
        Dcm_Runtime.LastFotaResult = DCM_FOTA_RESULT_SUCCESS;
        Dcm_Runtime.FotaReportPending = TRUE;
    }

    ResponsePayload[0] = ResetType;

    Dcm_SendPositiveResponse(
        DCM_SID_ECU_RESET,
        ResponsePayload,
        1U
    );
}

static void Dcm_HandleTesterPresent(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
)
{
    uint8 SubFunction;
    uint8 ResponsePayload[1];

    if (RequestLength < 2U)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_TESTER_PRESENT,
            DCM_NRC_INCORRECT_MESSAGE_LENGTH
        );
        return;
    }

    SubFunction = RequestDataPtr[1] & 0x7FU;

    if (SubFunction != 0x00U)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_TESTER_PRESENT,
            DCM_NRC_SUBFUNCTION_NOT_SUPPORTED
        );
        return;
    }

    ResponsePayload[0] = SubFunction;

    Dcm_SendPositiveResponse(
        DCM_SID_TESTER_PRESENT,
        ResponsePayload,
        1U
    );
}

static void Dcm_HandleRequestDownload(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
)
{
    uint8 ResponsePayload[4];

    /*
     * 최소 형식만 검사한다.
     *
     * 실제 UDS RequestDownload는:
     * SID
     * dataFormatIdentifier
     * addressAndLengthFormatIdentifier
     * memoryAddress
     * memorySize
     *
     * 로 구성되지만, 지금은 프로젝트 테스트용으로 상세 memoryAddress 해석은 생략한다.
     */
    if (RequestLength < 5U)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_REQUEST_DOWNLOAD,
            DCM_NRC_INCORRECT_MESSAGE_LENGTH
        );
        return;
    }

    if (Dcm_Runtime.CurrentSession != DCM_SESSION_EXTENDED)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_REQUEST_DOWNLOAD,
            DCM_NRC_CONDITIONS_NOT_CORRECT
        );
        return;
    }

    if (Dcm_IsFotaDownloadStartState(Dcm_Runtime.FotaState) == FALSE)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_REQUEST_DOWNLOAD,
            DCM_NRC_REQUEST_SEQUENCE_ERROR
        );
        return;
    }

    Dcm_ResetFotaDownloadContext();

    Dcm_Runtime.FotaState = DCM_FOTA_STATE_DOWNLOAD_ACCEPTED;
    Dcm_Runtime.LastFotaResult = DCM_FOTA_RESULT_NONE;

    /*
     * Positive Response 0x74:
     *
     * Byte 0: lengthFormatIdentifier
     * Byte 1~3: maxNumberOfBlockLength
     *
     */
    ResponsePayload[0] = DCM_LENGTH_FORMAT_MAX_BLOCK_LENGTH;

    Dcm_WriteUint24BigEndian(
        &ResponsePayload[1],
        DCM_MAX_TRANSFER_BLOCK_LENGTH
    );

    Dcm_SendPositiveResponse(
        DCM_SID_REQUEST_DOWNLOAD,
        ResponsePayload,
        4U
    );
}

static void Dcm_HandleTransferData(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
)
{
    uint8 BlockSequenceCounter;
    uint8 ResponsePayload[1];

    if (RequestLength < 2U)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_TRANSFER_DATA,
            DCM_NRC_INCORRECT_MESSAGE_LENGTH
        );
        return;
    }

    if (RequestLength > DCM_MAX_TRANSFER_BLOCK_LENGTH)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_TRANSFER_DATA,
            DCM_NRC_REQUEST_OUT_OF_RANGE
        );
        return;
    }

    if ((Dcm_Runtime.FotaState != DCM_FOTA_STATE_DOWNLOAD_ACCEPTED) &&
        (Dcm_Runtime.FotaState != DCM_FOTA_STATE_TRANSFER_IN_PROGRESS))
    {
        Dcm_SendNegativeResponse(
            DCM_SID_TRANSFER_DATA,
            DCM_NRC_REQUEST_SEQUENCE_ERROR
        );
        return;
    }

    BlockSequenceCounter = RequestDataPtr[1];

    if (BlockSequenceCounter != Dcm_Runtime.ExpectedBlockSequenceCounter)
    {
        Dcm_Runtime.FotaState = DCM_FOTA_STATE_FAILED;
        Dcm_Runtime.LastFotaResult = DCM_FOTA_RESULT_TRANSFER_FAILED;

        Dcm_SendNegativeResponse(
            DCM_SID_TRANSFER_DATA,
            DCM_NRC_REQUEST_SEQUENCE_ERROR
        );
        return;
    }

    /*
     * 실제 구현에서는 여기서 RequestDataPtr[2]부터 flash write buffer로 넘긴다.
     * 현재는 데이터 수신 성공으로만 처리한다.
     */
    Dcm_Runtime.FotaState = DCM_FOTA_STATE_TRANSFER_IN_PROGRESS;

    if (Dcm_Runtime.ExpectedBlockSequenceCounter == DCM_MAX_BLOCK_SEQUENCE_COUNTER)
    {
        Dcm_Runtime.ExpectedBlockSequenceCounter = 0U;
    }
    else
    {
        Dcm_Runtime.ExpectedBlockSequenceCounter++;
    }

    ResponsePayload[0] = BlockSequenceCounter;

    Dcm_SendPositiveResponse(
        DCM_SID_TRANSFER_DATA,
        ResponsePayload,
        1U
    );
}

static void Dcm_HandleRequestTransferExit(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
)
{
    (void)RequestDataPtr;

    if (RequestLength < 1U)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_REQUEST_TRANSFER_EXIT,
            DCM_NRC_INCORRECT_MESSAGE_LENGTH
        );
        return;
    }

    if (Dcm_Runtime.FotaState != DCM_FOTA_STATE_TRANSFER_IN_PROGRESS)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_REQUEST_TRANSFER_EXIT,
            DCM_NRC_REQUEST_SEQUENCE_ERROR
        );
        return;
    }

    Dcm_Runtime.FotaState = DCM_FOTA_STATE_TRANSFER_COMPLETED;

    Dcm_SendPositiveResponse(
        DCM_SID_REQUEST_TRANSFER_EXIT,
        NULL_PTR,
        0U
    );
}

static void Dcm_HandleRoutineControl(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
)
{
    uint8 RoutineControlType;
    uint16 RoutineId;
    uint8 ResponsePayload[3];

    if (RequestLength < 4U)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_ROUTINE_CONTROL,
            DCM_NRC_INCORRECT_MESSAGE_LENGTH
        );
        return;
    }

    RoutineControlType = RequestDataPtr[1] & 0x7FU;

    RoutineId = Dcm_ParseUint16BigEndian(
        &RequestDataPtr[2]
    );

    if (RoutineControlType != DCM_ROUTINE_CONTROL_START_ROUTINE)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_ROUTINE_CONTROL,
            DCM_NRC_SUBFUNCTION_NOT_SUPPORTED
        );
        return;
    }

    switch (RoutineId)
    {
        case DCM_RID_VERIFY_IMAGE:
        {
            if (Dcm_Runtime.FotaState != DCM_FOTA_STATE_TRANSFER_COMPLETED)
            {
                Dcm_SendNegativeResponse(
                    DCM_SID_ROUTINE_CONTROL,
                    DCM_NRC_REQUEST_SEQUENCE_ERROR
                );
                return;
            }

            /*
             * 실제 구현에서는 이미지 해시/CRC/서명 검증을 수행해야 한다.
             * 현재는 검증 성공으로 가정한다.
             */
            Dcm_Runtime.FotaState = DCM_FOTA_STATE_VERIFIED;

            break;
        }

        case DCM_RID_ACTIVATE_IMAGE:
        {
            if (Dcm_Runtime.FotaState != DCM_FOTA_STATE_PROGRAMMING_SESSION)
            {
                Dcm_SendNegativeResponse(
                    DCM_SID_ROUTINE_CONTROL,
                    DCM_NRC_REQUEST_SEQUENCE_ERROR
                );
                return;
            }

            /*
             * 실제 구현에서는 부트 플래그 변경, A/B partition 선택 등을 수행한다.
             * 현재는 activation pending 상태로만 둔다.
             */
            Dcm_Runtime.FotaState = DCM_FOTA_STATE_ACTIVATION_PENDING;

            break;
        }

        case DCM_RID_ROLLBACK_IMAGE:
        {
            Dcm_Runtime.FotaState = DCM_FOTA_STATE_ROLLBACK_DONE;
            Dcm_Runtime.LastFotaResult = DCM_FOTA_RESULT_ROLLBACK_DONE;

            Dcm_Runtime.FotaReportPending = TRUE;
            Dcm_Runtime.FotaIdleTransitionPending = FALSE;

            break;
        }

        default:
        {
            Dcm_SendNegativeResponse(
                DCM_SID_ROUTINE_CONTROL,
                DCM_NRC_REQUEST_OUT_OF_RANGE
            );
            return;
        }
    }

    ResponsePayload[0] = RoutineControlType;

    Dcm_WriteUint16BigEndian(
        &ResponsePayload[1],
        RoutineId
    );

    Dcm_SendPositiveResponse(
        DCM_SID_ROUTINE_CONTROL,
        ResponsePayload,
        3U
    );
}

static void Dcm_HandleReadDataByIdentifier(
    const uint8* RequestDataPtr,
    PduLengthType RequestLength
)
{
    uint16 Did;
    uint8 ResponsePayload[4];

    /*
     * Request format:
     *   22 [DID_H] [DID_L]
     *
     * Example:
     *   22 F1 80
     */
    if (RequestLength != 3U)
    {
        Dcm_SendNegativeResponse(
            DCM_SID_READ_DATA_BY_IDENTIFIER,
            DCM_NRC_INCORRECT_MESSAGE_LENGTH
        );
        return;
    }

    Did = (uint16)(((uint16)RequestDataPtr[1] << 8U) | RequestDataPtr[2]);

    switch (Did)
    {
        case DCM_DID_FOTA_STATUS:
        {
            /*
             * Positive response format:
             *   62 F1 80 [FotaState] [LastFotaResult]
             *
             * Dcm_SendPositiveResponse()가 SID를 앞에 붙여준다고 가정하면,
             * payload에는 DID + data만 넣는다.
             */
            ResponsePayload[0] = RequestDataPtr[1]; /* DID_H = F1 */
            ResponsePayload[1] = RequestDataPtr[2]; /* DID_L = 80 */
            ResponsePayload[2] = (uint8)Dcm_Runtime.FotaState;
            ResponsePayload[3] = (uint8)Dcm_Runtime.LastFotaResult;

            Dcm_SendPositiveResponse(
                DCM_SID_READ_DATA_BY_IDENTIFIER,
                ResponsePayload,
                4U
            );

            if (((Dcm_Runtime.FotaState == DCM_FOTA_STATE_ACTIVATED) ||
                (Dcm_Runtime.FotaState == DCM_FOTA_STATE_ROLLBACK_DONE)) &&
                (Dcm_Runtime.FotaReportPending == TRUE))
            {
                Dcm_Runtime.FotaReportPending = FALSE;
                Dcm_Runtime.FotaIdleTransitionPending = TRUE;
            }

            break;
        }

        default:
        {
            Dcm_SendNegativeResponse(
                DCM_SID_READ_DATA_BY_IDENTIFIER,
                DCM_NRC_REQUEST_OUT_OF_RANGE
            );
            break;
        }
    }
}

/*********************************************************************************************************************/
/*------------------------------------------------Response Functions-------------------------------------------------*/
/*********************************************************************************************************************/

static void Dcm_SendPositiveResponse(
    uint8 Sid,
    const uint8* PayloadPtr,
    PduLengthType PayloadLength
)
{
    PduLengthType ResponseLength;

    ResponseLength = (PduLengthType)(1U + PayloadLength);

    if (ResponseLength > DCM_TX_BUFFER_SIZE)
    {
        return;
    }

    Dcm_Runtime.TxBuffer[0] = (uint8)(Sid + DCM_POSITIVE_RESPONSE_OFFSET);

    if ((PayloadPtr != NULL_PTR) && (PayloadLength > 0U))
    {
        memcpy(
            &Dcm_Runtime.TxBuffer[1],
            PayloadPtr,
            PayloadLength
        );
    }

    Dcm_SendResponse(
        Dcm_Runtime.TxBuffer,
        ResponseLength
    );
}

static void Dcm_SendNegativeResponse(
    uint8 Sid,
    uint8 Nrc
)
{
    Dcm_Runtime.TxBuffer[0] = DCM_NEGATIVE_RESPONSE_SID;
    Dcm_Runtime.TxBuffer[1] = Sid;
    Dcm_Runtime.TxBuffer[2] = Nrc;

    Dcm_SendResponse(
        Dcm_Runtime.TxBuffer,
        3U
    );
}

static Std_ReturnType Dcm_SendResponse(
    const uint8* ResponseDataPtr,
    PduLengthType ResponseLength
)
{
    PduInfoType PduInfo;

    if ((ResponseDataPtr == NULL_PTR) || (ResponseLength == 0U))
    {
        return E_NOT_OK;
    }

    PduInfo.SduDataPtr = (uint8*)ResponseDataPtr;
    PduInfo.SduLength = ResponseLength;

    DCM_DEBUG_PRINTF(
        "[Dcm][TX] DcmTxPduId=%u",
        (unsigned int)DCM_TXPDU_DIAG_RES
    );
    DCM_DEBUG_PRINT_PDU("", &PduInfo);

    return PduR_DcmTransmit(
        DCM_TXPDU_DIAG_RES,
        &PduInfo
    );
}

/*********************************************************************************************************************/
/*------------------------------------------------Utility Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static uint16 Dcm_ParseUint16BigEndian(
    const uint8* DataPtr
)
{
    return (uint16)(((uint16)DataPtr[0] << 8U) |
                    ((uint16)DataPtr[1]));
}

static void Dcm_WriteUint16BigEndian(
    uint8* DataPtr,
    uint16 Value
)
{
    DataPtr[0] = (uint8)((Value >> 8U) & 0xFFU);
    DataPtr[1] = (uint8)(Value & 0xFFU);
}

static void Dcm_WriteUint24BigEndian(
    uint8* DataPtr,
    uint32 Value
)
{
    DataPtr[0] = (uint8)((Value >> 16U) & 0xFFU);
    DataPtr[1] = (uint8)((Value >> 8U) & 0xFFU);
    DataPtr[2] = (uint8)(Value & 0xFFU);
}

static void Dcm_ResetFotaDownloadContext(void)
{
    Dcm_Runtime.ExpectedBlockSequenceCounter = 1U;
    Dcm_Runtime.FotaIdleTransitionPending = FALSE;
    Dcm_Runtime.FotaReportPending = FALSE;
}

static boolean Dcm_IsFotaDownloadStartState(
    Dcm_FotaStateType FotaState
)
{
    boolean IsDownloadStartState = FALSE;

    if ((FotaState == DCM_FOTA_STATE_IDLE) ||
        (FotaState == DCM_FOTA_STATE_EXTENDED_SESSION) ||
        (FotaState == DCM_FOTA_STATE_ROLLBACK_DONE))
    {
        IsDownloadStartState = TRUE;
    }

    return IsDownloadStartState;
}

