#include "Sota.h"

#include "Metadata/Sota_Metadata.h"
#include "Swap/Sota_SwapManager.h"
#include "Transport/Sota_CanFdIf.h"
#include "Update/Sota_UpdateManager.h"

#ifndef SOTA_CANFD_RESPONSE_ID
#define SOTA_CANFD_RESPONSE_ID (0U)
#endif

static boolean Sota_DispatchCanFdMessage(const SotaCanFd_ParsedMessage_t *message,
                                         SotaCanFd_TxFrame_t *responseFrame);
static boolean Sota_BuildCommandResponse(SotaCanFd_Command_t command,
                                         SotaCanFd_ResponseCode_t responseCode,
                                         uint32 seq,
                                         SotaCanFd_TxFrame_t *responseFrame);
static boolean Sota_BuildParseErrorResponse(const SotaCanFd_RxFrame_t *rxFrame,
                                            SotaCanFd_ParseResult_t parseResult,
                                            SotaCanFd_TxFrame_t *responseFrame);
static SotaCanFd_ResponseCode_t Sota_MapParseError(SotaCanFd_ParseResult_t parseResult);
static void Sota_QueueCanFdTx(const SotaCanFd_TxFrame_t *responseFrame);
static void Sota_CopyIfFrameToTransportFrame(const SotaCanFdIf_Frame_t *ifFrame,
                                             SotaCanFd_RxFrame_t *transportFrame);
static void Sota_CopyTransportFrameToIfFrame(const SotaCanFd_TxFrame_t *transportFrame,
                                             SotaCanFdIf_Frame_t *ifFrame);
static void Sota_HandleBootMetadata(SotaMetadata_Record_t *record);
static boolean Sota_IsKnownBank(uint8 bank);

void Sota_Init(void)
{
    SotaCanFdIf_Init();
    SotaCanFdTransport_Init();
    SotaUpdateManager_Init();
}

void Sota_MainFunction_10ms(void)
{
    SotaCanFdIf_Frame_t ifFrame;
    SotaCanFd_RxFrame_t rxFrame;

    /*
     * Drain queued CAN-FD RX frames from background context. ISR code should
     * only queue frames through SotaCanFdIf_OnRxIsr(); flash work is advanced
     * below by SotaUpdateManager_RunStep().
     */
    while (SotaCanFdIf_Read(&ifFrame) == TRUE)
    {
        Sota_CopyIfFrameToTransportFrame(&ifFrame, &rxFrame);
        Sota_OnCanFdRxFrame(&rxFrame);
    }

    SotaUpdateManager_RunStep();
}

void Sota_OnCanFdRxFrame(const SotaCanFd_RxFrame_t *frame)
{
    SotaCanFd_ParsedMessage_t message;
    SotaCanFd_TxFrame_t responseFrame;
    SotaCanFd_ParseResult_t parseResult;
    boolean responseReady;

    if (frame == NULL_PTR)
    {
        return;
    }

    parseResult = SotaCanFdTransport_ParseRxFrame(frame, &message);
    if (parseResult == SOTA_CANFD_PARSE_OK)
    {
        responseReady = Sota_DispatchCanFdMessage(&message, &responseFrame);
    }
    else
    {
        responseReady = Sota_BuildParseErrorResponse(frame,
                                                     parseResult,
                                                     &responseFrame);
    }

    if (responseReady == TRUE)
    {
        Sota_QueueCanFdTx(&responseFrame);
    }
}

void Sota_BootCheck(void)
{
    SotaMetadata_Record_t record;

    if (SotaMetadata_LoadLatest(&record) != SOTA_METADATA_STATUS_OK)
    {
        return;
    }

    Sota_HandleBootMetadata(&record);

    if (SotaMetadata_LoadLatest(&record) != SOTA_METADATA_STATUS_OK)
    {
        return;
    }

    SotaUpdateManager_RestoreFromMetadata(&record);
}

static boolean Sota_DispatchCanFdMessage(const SotaCanFd_ParsedMessage_t *message,
                                         SotaCanFd_TxFrame_t *responseFrame)
{
    SotaCanFd_ResponseCode_t responseCode = SOTA_RSP_INTERNAL_ERROR;
    SotaCanFd_StatusResponse_t statusResponse;
    uint32 seq = 0U;

    if ((message == NULL_PTR) || (responseFrame == NULL_PTR))
    {
        return FALSE;
    }

    switch (message->command)
    {
    case SOTA_CMD_BEGIN:
        responseCode = SotaUpdateManager_HandleBegin(&message->begin);
        break;

    case SOTA_CMD_DATA:
        responseCode = SotaUpdateManager_HandleData(&message->data);
        seq = message->data.seq;
        break;

    case SOTA_CMD_END:
        responseCode = SotaUpdateManager_HandleEnd(&message->control);
        seq = message->control.seq;
        break;

    case SOTA_CMD_STATUS:
        responseCode = SotaUpdateManager_HandleStatus(&message->control,
                                                      &statusResponse);
        if ((responseCode == SOTA_RSP_OK) || (responseCode == SOTA_RSP_PENDING))
        {
            statusResponse.responseCode = responseCode;
            return SotaCanFdTransport_BuildStatusResponse(SOTA_CANFD_RESPONSE_ID,
                                                          &statusResponse,
                                                          responseFrame);
        }
        else
        {
            return Sota_BuildCommandResponse(message->command,
                                             responseCode,
                                             message->control.seq,
                                             responseFrame);
        }

    case SOTA_CMD_VERIFY:
        responseCode = SotaUpdateManager_HandleVerify(&message->control);
        seq = message->control.seq;
        break;

    case SOTA_CMD_ACTIVATE:
        responseCode = SotaUpdateManager_HandleActivate(&message->control);
        seq = message->control.seq;
        break;

    case SOTA_CMD_COMMIT:
        responseCode = SotaUpdateManager_HandleCommit(&message->control);
        seq = message->control.seq;
        break;

    case SOTA_CMD_ROLLBACK:
        responseCode = SotaUpdateManager_HandleRollback(&message->control);
        seq = message->control.seq;
        break;

    case SOTA_CMD_ABORT:
        responseCode = SotaUpdateManager_HandleAbort(&message->control);
        seq = message->control.seq;
        break;

    default:
        responseCode = SOTA_RSP_INVALID_PARAM;
        break;
    }

    return Sota_BuildCommandResponse(message->command,
                                     responseCode,
                                     seq,
                                     responseFrame);
}

static boolean Sota_BuildCommandResponse(SotaCanFd_Command_t command,
                                         SotaCanFd_ResponseCode_t responseCode,
                                         uint32 seq,
                                         SotaCanFd_TxFrame_t *responseFrame)
{
    boolean built;

    if (responseCode == SOTA_RSP_OK)
    {
        built = SotaCanFdTransport_BuildPositiveResponse(SOTA_CANFD_RESPONSE_ID,
                                                         command,
                                                         seq,
                                                         responseFrame);
    }
    else if (responseCode == SOTA_RSP_PENDING)
    {
        built = SotaCanFdTransport_BuildPendingResponse(SOTA_CANFD_RESPONSE_ID,
                                                        command,
                                                        seq,
                                                        0U,
                                                        responseFrame);
    }
    else
    {
        built = SotaCanFdTransport_BuildNegativeResponse(SOTA_CANFD_RESPONSE_ID,
                                                         command,
                                                         responseCode,
                                                         seq,
                                                         0U,
                                                         responseFrame);
    }

    return built;
}

static boolean Sota_BuildParseErrorResponse(const SotaCanFd_RxFrame_t *rxFrame,
                                            SotaCanFd_ParseResult_t parseResult,
                                            SotaCanFd_TxFrame_t *responseFrame)
{
    SotaCanFd_Command_t command = SOTA_CMD_STATUS;
    boolean built;

    if ((rxFrame == NULL_PTR) || (responseFrame == NULL_PTR))
    {
        return FALSE;
    }

    if (rxFrame->dlc > SOTA_CANFD_REQ_COMMAND_OFFSET)
    {
        command = (SotaCanFd_Command_t)rxFrame->data[SOTA_CANFD_REQ_COMMAND_OFFSET];
    }

    built = SotaCanFdTransport_BuildNegativeResponse(SOTA_CANFD_RESPONSE_ID,
                                                     command,
                                                     Sota_MapParseError(parseResult),
                                                     0U,
                                                     (uint32)parseResult,
                                                     responseFrame);
    if (built == FALSE)
    {
        built = SotaCanFdTransport_BuildNegativeResponse(SOTA_CANFD_RESPONSE_ID,
                                                         SOTA_CMD_STATUS,
                                                         SOTA_RSP_INVALID_PARAM,
                                                         0U,
                                                         (uint32)parseResult,
                                                         responseFrame);
    }

    return built;
}

static SotaCanFd_ResponseCode_t Sota_MapParseError(SotaCanFd_ParseResult_t parseResult)
{
    SotaCanFd_ResponseCode_t responseCode;

    switch (parseResult)
    {
    case SOTA_CANFD_PARSE_UNSUPPORTED_VERSION:
    case SOTA_CANFD_PARSE_UNKNOWN_COMMAND:
    case SOTA_CANFD_PARSE_FRAME_TOO_SHORT:
    case SOTA_CANFD_PARSE_UNSUPPORTED_DLC:
    case SOTA_CANFD_PARSE_PAYLOAD_TOO_LONG:
        responseCode = SOTA_RSP_INVALID_PARAM;
        break;

    case SOTA_CANFD_PARSE_INVALID_PARAM:
    default:
        responseCode = SOTA_RSP_INTERNAL_ERROR;
        break;
    }

    return responseCode;
}

static void Sota_QueueCanFdTx(const SotaCanFd_TxFrame_t *responseFrame)
{
    SotaCanFdIf_Frame_t ifFrame;

    if (responseFrame == NULL_PTR)
    {
        return;
    }

    Sota_CopyTransportFrameToIfFrame(responseFrame, &ifFrame);
    (void)SotaCanFdIf_Write(&ifFrame);
}

static void Sota_CopyIfFrameToTransportFrame(const SotaCanFdIf_Frame_t *ifFrame,
                                             SotaCanFd_RxFrame_t *transportFrame)
{
    uint8 index;

    if ((ifFrame == NULL_PTR) || (transportFrame == NULL_PTR))
    {
        return;
    }

    transportFrame->id = ifFrame->id;
    transportFrame->dlc = ifFrame->dlc;
    for (index = 0U; index < SOTA_CANFD_MAX_PAYLOAD_SIZE; index++)
    {
        transportFrame->data[index] = ifFrame->data[index];
    }
}

static void Sota_CopyTransportFrameToIfFrame(const SotaCanFd_TxFrame_t *transportFrame,
                                             SotaCanFdIf_Frame_t *ifFrame)
{
    uint8 index;

    if ((transportFrame == NULL_PTR) || (ifFrame == NULL_PTR))
    {
        return;
    }

    ifFrame->id = transportFrame->id;
    ifFrame->dlc = transportFrame->dlc;
    for (index = 0U; index < SOTA_CANFD_MAX_PAYLOAD_SIZE; index++)
    {
        ifFrame->data[index] = transportFrame->data[index];
    }
}

static void Sota_HandleBootMetadata(SotaMetadata_Record_t *record)
{
    SotaSwapManager_Result_t swapResult;
    uint16 swapEntryIndex;
    uint8 currentBank;
    uint8 rollbackTarget;
    boolean activationState;
    boolean rollbackState;

    if (record == NULL_PTR)
    {
        return;
    }

    currentBank = (uint8)SotaSwapManager_GetCurrentBank();
    record->currentActiveBank = currentBank;
    rollbackTarget = record->activeBankBeforeUpdate;

    if (record->maxBootAttempt == 0U)
    {
        record->maxBootAttempt = 1U;
    }

    if ((record->state == SOTA_METADATA_STATE_ERROR) &&
        (record->rollbackReason == SOTA_UPDATE_ERROR_BOOT_ATTEMPT_EXCEEDED) &&
        (Sota_IsKnownBank(rollbackTarget) == TRUE) &&
        (Sota_IsKnownBank(currentBank) == TRUE) &&
        (rollbackTarget == currentBank))
    {
        record->targetBank = rollbackTarget;
        record->state = SOTA_METADATA_STATE_ROLLED_BACK;
        (void)SotaMetadata_Append(record);
        return;
    }

    activationState =
        ((record->state == SOTA_METADATA_STATE_SWAP_ARM_REQUESTED) ||
         (record->state == SOTA_METADATA_STATE_SWAP_ARMED) ||
         (record->state == SOTA_METADATA_STATE_TESTING_AFTER_BOOT)) ? TRUE : FALSE;
    rollbackState =
        ((record->state == SOTA_METADATA_STATE_ROLLBACK_REQUESTED) ||
         (record->state == SOTA_METADATA_STATE_ROLLBACK_ARMED)) ? TRUE : FALSE;

    if ((activationState == FALSE) && (rollbackState == FALSE))
    {
        return;
    }

    if ((rollbackState == TRUE) &&
        (Sota_IsKnownBank(currentBank) == TRUE) &&
        (((Sota_IsKnownBank(record->targetBank) == TRUE) &&
          (record->targetBank == currentBank)) ||
         ((Sota_IsKnownBank(rollbackTarget) == TRUE) &&
          (rollbackTarget == currentBank))))
    {
        if ((Sota_IsKnownBank(rollbackTarget) == TRUE) &&
            (rollbackTarget == currentBank))
        {
            record->targetBank = rollbackTarget;
        }
        record->state = SOTA_METADATA_STATE_ROLLED_BACK;
        (void)SotaMetadata_Append(record);
        return;
    }

    if ((rollbackState == TRUE) &&
        (record->state == SOTA_METADATA_STATE_ROLLBACK_REQUESTED))
    {
        return;
    }

    if (rollbackState == TRUE)
    {
        record->state = SOTA_METADATA_STATE_ERROR;
        record->lastError = SOTA_UPDATE_ERROR_BOOT_BANK_MISMATCH;
        (void)SotaMetadata_Append(record);
        return;
    }

    if ((record->state == SOTA_METADATA_STATE_SWAP_ARM_REQUESTED) &&
        (Sota_IsKnownBank(record->targetBank) == TRUE) &&
        (Sota_IsKnownBank(currentBank) == TRUE) &&
        (record->targetBank != currentBank))
    {
        return;
    }

    if ((activationState == TRUE) &&
        ((Sota_IsKnownBank(record->targetBank) == FALSE) ||
         (Sota_IsKnownBank(currentBank) == FALSE)))
    {
        record->state = SOTA_METADATA_STATE_ERROR;
        record->lastError = SOTA_UPDATE_ERROR_BOOT_BANK_MISMATCH;
        (void)SotaMetadata_Append(record);
        return;
    }

    if (record->bootAttempt < 0xFFU)
    {
        record->bootAttempt++;
    }

    if ((Sota_IsKnownBank(record->targetBank) == TRUE) &&
        (Sota_IsKnownBank(currentBank) == TRUE) &&
        (record->targetBank != currentBank))
    {
        record->state = SOTA_METADATA_STATE_ERROR;
        record->lastError = SOTA_UPDATE_ERROR_BOOT_BANK_MISMATCH;
        (void)SotaMetadata_Append(record);
        return;
    }

    if (record->bootAttempt > record->maxBootAttempt)
    {
        record->rollbackReason = SOTA_UPDATE_ERROR_BOOT_ATTEMPT_EXCEEDED;
        record->lastError = SOTA_UPDATE_ERROR_BOOT_ATTEMPT_EXCEEDED;

        if ((Sota_IsKnownBank(rollbackTarget) == TRUE) &&
            (Sota_IsKnownBank(currentBank) == TRUE) &&
            (rollbackTarget != currentBank))
        {
            swapResult =
                SotaSwapManager_ArmSwapToBank((SotaTc37x_PflashBank_t)rollbackTarget);
        }
        else
        {
            rollbackTarget = (uint8)SotaSwapManager_GetInactiveBank();
            swapResult = SotaSwapManager_ArmRollbackToPreviousBank();
        }

        if (swapResult == SOTA_SWAP_MANAGER_RESULT_OK)
        {
            record->state = SOTA_METADATA_STATE_ROLLBACK_ARMED;
            if (Sota_IsKnownBank(rollbackTarget) == TRUE)
            {
                record->targetBank = rollbackTarget;
            }
            swapEntryIndex = SotaSwapManager_GetLastSwapEntryIndex();
            if (swapEntryIndex != 0xFFFFU)
            {
                record->swapEntryIndex = swapEntryIndex;
            }
        }
        else
        {
            record->state = SOTA_METADATA_STATE_ERROR;
            record->lastError =
                (swapResult == SOTA_SWAP_MANAGER_RESULT_NO_FREE_ENTRY) ?
                SOTA_UPDATE_ERROR_SWAP_LOG_FULL :
                SOTA_UPDATE_ERROR_SWAP_ARM_FAILED;
        }

        (void)SotaMetadata_Append(record);
        return;
    }

    record->state = SOTA_METADATA_STATE_TESTING_AFTER_BOOT;
    (void)SotaMetadata_Append(record);
}

static boolean Sota_IsKnownBank(uint8 bank)
{
    return ((bank == (uint8)SOTA_TC37X_PFLASH_BANK_PF0) ||
            (bank == (uint8)SOTA_TC37X_PFLASH_BANK_PF1)) ? TRUE : FALSE;
}
