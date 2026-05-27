#include "Sota_UpdateManager.h"

#include <string.h>

#include "Sota_Image.h"
#include "Sota_UpdateCore.h"
#include "../Metadata/Sota_Metadata.h"
#include "../Swap/Sota_SwapManager.h"

#define SOTA_UPDATE_MANAGER_INVALID_BANK        (0xFFU)
#define SOTA_UPDATE_MANAGER_METADATA_ERROR_BASE (0x4D450000U)

typedef struct
{
    SotaUpdateManager_State_t state;
    SotaCanFd_EcuType_t ecuType;
    uint8 activeBankBeforeUpdate;
    uint8 targetBank;
    uint32 imageVersion;
    uint32 imageSize;
    uint32 imageCrc;
    uint32 manifestCrc;
    uint32 flags;
    uint32 receivedBytes;
    uint32 programmedBytes;
    uint32 verifiedBytes;
    uint32 expectedSeq;
    uint32 expectedOffset;
    uint32 lastSeq;
    uint32 lastOffset;
    uint32 lastError;
    uint32 rollbackReason;
    boolean transferExitRequested;
    boolean pendingChunkValid;
    SotaCanFd_DataRequest_t pendingChunk;
} SotaUpdateManager_Context_t;

static SotaUpdateManager_Context_t g_sotaUpdateManager;
static boolean g_metadataStatePending = FALSE;
static boolean g_metadataProgressPending = FALSE;
static boolean g_metadataErrorPending = FALSE;
static SotaMetadata_State_t g_pendingMetadataState = SOTA_METADATA_STATE_IDLE;

static boolean SotaUpdateManager_IsStartAllowed(void);
static boolean SotaUpdateManager_IsAbortAllowed(void);
static boolean SotaUpdateManager_IsBusyState(SotaUpdateManager_State_t state);
static boolean SotaUpdateManager_IsRollbackAllowedFromError(void);
static boolean SotaUpdateManager_IsKnownBank(uint8 bank);
static uint8 SotaUpdateManager_GetRollbackTargetBank(void);
static SotaCanFd_ResponseCode_t SotaUpdateManager_ValidateBegin(const SotaCanFd_BeginRequest_t *request);
static boolean SotaUpdateManager_IsValidData(const SotaCanFd_DataRequest_t *request);
static void SotaUpdateManager_BuildManifest(const SotaCanFd_BeginRequest_t *request,
                                            SotaImage_Manifest_t *manifest);
static SotaCanFd_ResponseCode_t SotaUpdateManager_MapImageStatus(SotaImage_Status_t status);
static void SotaUpdateManager_ResetContext(void);
static void SotaUpdateManager_SetState(SotaUpdateManager_State_t state);
static void SotaUpdateManager_SetRuntimeState(SotaUpdateManager_State_t state);
static void SotaUpdateManager_EnterError(SotaCanFd_ResponseCode_t errorCode);
static void SotaUpdateManager_EnterErrorWithCode(SotaCanFd_ResponseCode_t responseCode,
                                                 uint32 detailCode);
static void SotaUpdateManager_RequestMetadataState(SotaMetadata_State_t state);
static void SotaUpdateManager_RequestMetadataProgress(void);
static void SotaUpdateManager_FlushMetadata(void);
static SotaMetadata_Status_t SotaUpdateManager_WriteMetadataSnapshot(SotaMetadata_State_t state);
static void SotaUpdateManager_SetDefaultMetadataRecord(SotaMetadata_Record_t *record);
static void SotaUpdateManager_ResetTrialMetadataForNewUpdate(SotaMetadata_Record_t *record,
                                                            SotaMetadata_State_t state);
static boolean SotaUpdateManager_MapRuntimeState(SotaMetadata_State_t metadataState,
                                                 SotaUpdateManager_State_t *outRuntimeState);
static boolean SotaUpdateManager_MapMetadataState(SotaUpdateManager_State_t updateState,
                                                  SotaMetadata_State_t *outMetadataState);
static uint32 SotaUpdateManager_MetadataError(SotaMetadata_Status_t status);
static SotaCanFd_ResponseCode_t SotaUpdateManager_MapCoreResult(SotaUpdateCore_Result_t result);
static SotaCanFd_ResponseCode_t SotaUpdateManager_MapSwapResult(SotaSwapManager_Result_t result);
static uint32 SotaUpdateManager_GetSwapErrorCode(SotaSwapManager_Result_t result);
static void SotaUpdateManager_RefreshCoreProgress(void);
static SotaCanFd_ResponseCode_t SotaUpdateManager_ProcessPendingChunk(void);
static SotaCanFd_ResponseCode_t SotaUpdateManager_RunVerifyStep(void);

void SotaUpdateManager_Init(void)
{
    SotaUpdateManager_ResetContext();
    SotaSwapManager_Init();
    (void)SotaMetadata_Init();
}

SotaCanFd_ResponseCode_t SotaUpdateManager_HandleBegin(const SotaCanFd_BeginRequest_t *request)
{
    SotaUpdateCore_Result_t coreResult;
    SotaCanFd_ResponseCode_t responseCode;

    if (request == NULL_PTR)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (request->command != SOTA_CMD_BEGIN)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (SotaUpdateManager_IsStartAllowed() == FALSE)
    {
        return SOTA_RSP_INVALID_STATE;
    }

    responseCode = SotaUpdateManager_ValidateBegin(request);
    if (responseCode != SOTA_RSP_OK)
    {
        return responseCode;
    }

    SotaUpdateManager_ResetContext();
    g_sotaUpdateManager.state = SOTA_UPDATE_MANAGER_STATE_SESSION_OPEN;
    g_sotaUpdateManager.ecuType = request->ecuType;
    g_sotaUpdateManager.activeBankBeforeUpdate =
        (uint8)SotaSwapManager_GetCurrentBank();
    g_sotaUpdateManager.targetBank = SOTA_UPDATE_MANAGER_INVALID_BANK;
    g_sotaUpdateManager.imageVersion = request->imageVersion;
    g_sotaUpdateManager.imageSize = request->imageSize;
    g_sotaUpdateManager.imageCrc = request->imageCrc;
    g_sotaUpdateManager.manifestCrc = request->manifestCrc;
    g_sotaUpdateManager.flags = request->flags;

    coreResult = SotaUpdateCore_Prepare(request->imageSize,
                                        request->imageCrc);
    SotaUpdateManager_RefreshCoreProgress();
    responseCode = SotaUpdateManager_MapCoreResult(coreResult);
    if (responseCode != SOTA_RSP_OK)
    {
        SotaUpdateManager_EnterError(responseCode);
        return responseCode;
    }

    /*
     * Queue the requested metadata state explicitly. The runtime state moves
     * directly to ERASE_PENDING, but DFlash writes are still flushed later from
     * RunStep() so CAN-FD RX dispatch never performs NVM programming.
     */
    SotaUpdateManager_RequestMetadataState(SOTA_METADATA_STATE_DOWNLOAD_REQUESTED);
    SotaUpdateManager_RequestMetadataProgress();
    SotaUpdateManager_SetRuntimeState(SOTA_UPDATE_MANAGER_STATE_ERASE_PENDING);

    return SOTA_RSP_PENDING;
}

SotaCanFd_ResponseCode_t SotaUpdateManager_HandleData(const SotaCanFd_DataRequest_t *request)
{
    if (request == NULL_PTR)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (request->command != SOTA_CMD_DATA)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if ((g_sotaUpdateManager.state != SOTA_UPDATE_MANAGER_STATE_RECEIVING) &&
        (g_sotaUpdateManager.state != SOTA_UPDATE_MANAGER_STATE_WRITE_PENDING))
    {
        return SOTA_RSP_INVALID_STATE;
    }

    if ((g_sotaUpdateManager.state == SOTA_UPDATE_MANAGER_STATE_WRITE_PENDING) &&
        (g_sotaUpdateManager.pendingChunkValid == TRUE))
    {
        if ((request->seq == g_sotaUpdateManager.pendingChunk.seq) &&
            (request->offset == g_sotaUpdateManager.pendingChunk.offset))
        {
            return SOTA_RSP_PENDING;
        }

        return SOTA_RSP_BUSY;
    }

    if (SotaUpdateManager_IsValidData(request) == FALSE)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    g_sotaUpdateManager.pendingChunk = *request;
    g_sotaUpdateManager.pendingChunkValid = TRUE;
    g_sotaUpdateManager.receivedBytes = request->offset + (uint32)request->payloadLen;
    g_sotaUpdateManager.lastSeq = request->seq;
    g_sotaUpdateManager.lastOffset = request->offset;
    SotaUpdateManager_SetRuntimeState(SOTA_UPDATE_MANAGER_STATE_WRITE_PENDING);

    return SOTA_RSP_PENDING;
}

SotaCanFd_ResponseCode_t SotaUpdateManager_HandleEnd(const SotaCanFd_ControlRequest_t *request)
{
    if (request == NULL_PTR)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (request->command != SOTA_CMD_END)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if ((g_sotaUpdateManager.state != SOTA_UPDATE_MANAGER_STATE_RECEIVING) &&
        (g_sotaUpdateManager.state != SOTA_UPDATE_MANAGER_STATE_WRITE_PENDING) &&
        (g_sotaUpdateManager.state != SOTA_UPDATE_MANAGER_STATE_PROGRAMMING))
    {
        return SOTA_RSP_INVALID_STATE;
    }

    if (g_sotaUpdateManager.receivedBytes != g_sotaUpdateManager.imageSize)
    {
        if ((g_sotaUpdateManager.pendingChunkValid == FALSE) ||
            ((g_sotaUpdateManager.pendingChunk.offset +
              (uint32)g_sotaUpdateManager.pendingChunk.payloadLen) !=
             g_sotaUpdateManager.imageSize))
        {
            return SOTA_RSP_INVALID_PARAM;
        }
    }

    g_sotaUpdateManager.transferExitRequested = TRUE;

    if ((g_sotaUpdateManager.state == SOTA_UPDATE_MANAGER_STATE_RECEIVING) &&
        (g_sotaUpdateManager.pendingChunkValid == FALSE))
    {
        SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_TRANSFER_EXIT_RECEIVED);
        return SOTA_RSP_OK;
    }

    return SOTA_RSP_PENDING;
}

SotaCanFd_ResponseCode_t SotaUpdateManager_HandleStatus(const SotaCanFd_ControlRequest_t *request,
                                                        SotaCanFd_StatusResponse_t *outStatus)
{
    SotaCanFd_ResponseCode_t status;

    if ((request == NULL_PTR) || (outStatus == NULL_PTR))
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (request->command != SOTA_CMD_STATUS)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    status = SotaUpdateManager_GetStatus(outStatus);
    outStatus->seq = request->seq;
    return status;
}

SotaCanFd_ResponseCode_t SotaUpdateManager_HandleVerify(const SotaCanFd_ControlRequest_t *request)
{
    if (request == NULL_PTR)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (request->command != SOTA_CMD_VERIFY)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (g_sotaUpdateManager.state == SOTA_UPDATE_MANAGER_STATE_VERIFIED)
    {
        return SOTA_RSP_OK;
    }

    if ((g_sotaUpdateManager.state == SOTA_UPDATE_MANAGER_STATE_VERIFY_PENDING) ||
        (g_sotaUpdateManager.state == SOTA_UPDATE_MANAGER_STATE_VERIFYING))
    {
        return SOTA_RSP_PENDING;
    }

    if (g_sotaUpdateManager.state != SOTA_UPDATE_MANAGER_STATE_TRANSFER_EXIT_RECEIVED)
    {
        return SOTA_RSP_INVALID_STATE;
    }

    SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_VERIFY_PENDING);
    return SOTA_RSP_PENDING;
}

SotaCanFd_ResponseCode_t SotaUpdateManager_HandleActivate(const SotaCanFd_ControlRequest_t *request)
{
    if (request == NULL_PTR)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (request->command != SOTA_CMD_ACTIVATE)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (g_sotaUpdateManager.state == SOTA_UPDATE_MANAGER_STATE_SWAP_ARMED)
    {
        return SOTA_RSP_OK;
    }

    if (g_sotaUpdateManager.state == SOTA_UPDATE_MANAGER_STATE_SWAP_ARM_REQUESTED)
    {
        return SOTA_RSP_PENDING;
    }

    if (g_sotaUpdateManager.state != SOTA_UPDATE_MANAGER_STATE_VERIFIED)
    {
        return SOTA_RSP_INVALID_STATE;
    }

    g_sotaUpdateManager.targetBank = (uint8)SotaSwapManager_GetInactiveBank();
    if (g_sotaUpdateManager.targetBank == SOTA_UPDATE_MANAGER_INVALID_BANK)
    {
        return SOTA_RSP_INVALID_STATE;
    }

    SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_SWAP_ARM_REQUESTED);
    return SOTA_RSP_PENDING;
}

SotaCanFd_ResponseCode_t SotaUpdateManager_HandleCommit(const SotaCanFd_ControlRequest_t *request)
{
    if (request == NULL_PTR)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (request->command != SOTA_CMD_COMMIT)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (g_sotaUpdateManager.state != SOTA_UPDATE_MANAGER_STATE_TESTING_AFTER_BOOT)
    {
        SotaUpdateManager_EnterErrorWithCode(SOTA_RSP_INVALID_STATE,
                                             SOTA_UPDATE_ERROR_COMMIT_INVALID_STATE);
        return SOTA_RSP_INVALID_STATE;
    }

    SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_COMMITTED);
    return SOTA_RSP_PENDING;
}

SotaCanFd_ResponseCode_t SotaUpdateManager_HandleRollback(const SotaCanFd_ControlRequest_t *request)
{
    uint8 rollbackTarget;

    if (request == NULL_PTR)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (request->command != SOTA_CMD_ROLLBACK)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if ((g_sotaUpdateManager.state != SOTA_UPDATE_MANAGER_STATE_TESTING_AFTER_BOOT) &&
        (g_sotaUpdateManager.state != SOTA_UPDATE_MANAGER_STATE_ERROR) &&
        (g_sotaUpdateManager.state != SOTA_UPDATE_MANAGER_STATE_ROLLBACK_REQUESTED))
    {
        return SOTA_RSP_INVALID_STATE;
    }

    if ((g_sotaUpdateManager.state == SOTA_UPDATE_MANAGER_STATE_ERROR) &&
        (SotaUpdateManager_IsRollbackAllowedFromError() == FALSE))
    {
        return SOTA_RSP_INVALID_STATE;
    }

    rollbackTarget = SotaUpdateManager_GetRollbackTargetBank();
    if (rollbackTarget == SOTA_UPDATE_MANAGER_INVALID_BANK)
    {
        return SOTA_RSP_INVALID_STATE;
    }

    g_sotaUpdateManager.targetBank = rollbackTarget;
    g_sotaUpdateManager.rollbackReason = request->seq;
    SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_ROLLBACK_REQUESTED);
    return SOTA_RSP_PENDING;
}

SotaCanFd_ResponseCode_t SotaUpdateManager_HandleAbort(const SotaCanFd_ControlRequest_t *request)
{
    if (request == NULL_PTR)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (request->command != SOTA_CMD_ABORT)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    if (SotaUpdateManager_IsAbortAllowed() == FALSE)
    {
        return SOTA_RSP_INVALID_STATE;
    }

    g_sotaUpdateManager.pendingChunkValid = FALSE;
    g_sotaUpdateManager.transferExitRequested = FALSE;
    (void)SotaUpdateCore_Abort();
    SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_ABORTED);

    return SOTA_RSP_OK;
}

void SotaUpdateManager_RunStep(void)
{
    SotaCanFd_ResponseCode_t result;
    SotaUpdateCore_Result_t coreResult;

    SotaUpdateManager_FlushMetadata();

    switch (g_sotaUpdateManager.state)
    {
    case SOTA_UPDATE_MANAGER_STATE_DOWNLOAD_REQUESTED:
        SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_ERASE_PENDING);
        break;

    case SOTA_UPDATE_MANAGER_STATE_ERASE_PENDING:
        SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_ERASING);
        /* fall through */
    case SOTA_UPDATE_MANAGER_STATE_ERASING:
        coreResult = SotaUpdateCore_EraseStep();
        SotaUpdateManager_RefreshCoreProgress();
        result = SotaUpdateManager_MapCoreResult(coreResult);
        if (result == SOTA_RSP_OK)
        {
            SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_RECEIVING);
        }
        else if ((result != SOTA_RSP_OK) && (result != SOTA_RSP_PENDING))
        {
            SotaUpdateManager_EnterError(result);
        }
        break;

    case SOTA_UPDATE_MANAGER_STATE_WRITE_PENDING:
        SotaUpdateManager_SetRuntimeState(SOTA_UPDATE_MANAGER_STATE_PROGRAMMING);
        /* fall through */
    case SOTA_UPDATE_MANAGER_STATE_PROGRAMMING:
        result = SotaUpdateManager_ProcessPendingChunk();
        if (result == SOTA_RSP_OK)
        {
            if ((g_sotaUpdateManager.transferExitRequested == TRUE) &&
                (g_sotaUpdateManager.receivedBytes == g_sotaUpdateManager.imageSize))
            {
                SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_TRANSFER_EXIT_RECEIVED);
            }
            else
            {
                SotaUpdateManager_SetRuntimeState(SOTA_UPDATE_MANAGER_STATE_RECEIVING);
            }
        }
        else if (result != SOTA_RSP_PENDING)
        {
            SotaUpdateManager_EnterError(result);
        }
        break;

    case SOTA_UPDATE_MANAGER_STATE_VERIFY_PENDING:
    case SOTA_UPDATE_MANAGER_STATE_VERIFYING:
        result = SotaUpdateManager_RunVerifyStep();
        if (result == SOTA_RSP_OK)
        {
            g_sotaUpdateManager.verifiedBytes = g_sotaUpdateManager.imageSize;
            SotaUpdateManager_RequestMetadataState(SOTA_METADATA_STATE_VERIFIED);
            SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_VERIFIED);
        }
        else if (result != SOTA_RSP_PENDING)
        {
            SotaUpdateManager_EnterErrorWithCode(result,
                                                 SOTA_UPDATE_ERROR_VERIFY_FAILED);
        }
        break;

    case SOTA_UPDATE_MANAGER_STATE_SWAP_ARM_REQUESTED:
    {
        SotaSwapManager_Result_t swapResult =
            SotaSwapManager_ArmSwapToBank(
                (SotaTc37x_PflashBank_t)g_sotaUpdateManager.targetBank);
        result = SotaUpdateManager_MapSwapResult(swapResult);
        if (result == SOTA_RSP_OK)
        {
            SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_SWAP_ARMED);
        }
        else
        {
            SotaUpdateManager_EnterErrorWithCode(result,
                                                 SotaUpdateManager_GetSwapErrorCode(swapResult));
        }
        break;
    }

    case SOTA_UPDATE_MANAGER_STATE_ROLLBACK_REQUESTED:
    {
        SotaSwapManager_Result_t swapResult =
            SotaSwapManager_ArmRollbackToPreviousBank();
        result = SotaUpdateManager_MapSwapResult(swapResult);
        if (result == SOTA_RSP_OK)
        {
            SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_ROLLBACK_ARMED);
        }
        else
        {
            SotaUpdateManager_EnterErrorWithCode(result,
                                                 SotaUpdateManager_GetSwapErrorCode(swapResult));
        }
        break;
    }

    default:
        break;
    }

    SotaUpdateManager_FlushMetadata();
}

SotaCanFd_ResponseCode_t SotaUpdateManager_GetStatus(SotaCanFd_StatusResponse_t *outStatus)
{
    if (outStatus == NULL_PTR)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    SotaUpdateManager_RefreshCoreProgress();

    (void)memset(outStatus, 0, sizeof(SotaCanFd_StatusResponse_t));

    if (g_sotaUpdateManager.state == SOTA_UPDATE_MANAGER_STATE_ERROR)
    {
        outStatus->responseCode = SOTA_RSP_INTERNAL_ERROR;
    }
    else if (SotaUpdateManager_IsBusyState(g_sotaUpdateManager.state) == TRUE)
    {
        outStatus->responseCode = SOTA_RSP_PENDING;
    }
    else
    {
        outStatus->responseCode = SOTA_RSP_OK;
    }

    outStatus->updateState = (uint8)g_sotaUpdateManager.state;
    outStatus->targetBank = g_sotaUpdateManager.targetBank;
    outStatus->receivedBytes = g_sotaUpdateManager.receivedBytes;
    outStatus->programmedBytes = g_sotaUpdateManager.programmedBytes;
    outStatus->verifiedBytes = g_sotaUpdateManager.verifiedBytes;
    outStatus->imageCrc = g_sotaUpdateManager.imageCrc;
    outStatus->lastError = g_sotaUpdateManager.lastError;
    outStatus->rollbackReason = g_sotaUpdateManager.rollbackReason;

    return outStatus->responseCode;
}

void SotaUpdateManager_RestoreFromMetadata(const SotaMetadata_Record_t *record)
{
    SotaUpdateManager_State_t runtimeState;

    /*
     * TODO: Define resume policy. Mid-update states need either full
     * SotaUpdateCore reconstruction from flash/metadata or explicit
     * invalidation to ERROR/IDLE on boot; partial resume is not implemented.
     */
    if (record == NULL_PTR)
    {
        return;
    }

    if (SotaUpdateManager_MapRuntimeState(record->state, &runtimeState) == FALSE)
    {
        return;
    }

    SotaUpdateManager_ResetContext();
    g_sotaUpdateManager.state = runtimeState;
    g_sotaUpdateManager.ecuType = (SotaCanFd_EcuType_t)record->ecuType;
    g_sotaUpdateManager.activeBankBeforeUpdate = record->activeBankBeforeUpdate;
    g_sotaUpdateManager.targetBank = record->targetBank;
    g_sotaUpdateManager.imageVersion = record->imageVersion;
    g_sotaUpdateManager.imageSize = record->imageSize;
    g_sotaUpdateManager.imageCrc = record->imageCrc;
    g_sotaUpdateManager.manifestCrc = record->manifestCrc;
    g_sotaUpdateManager.receivedBytes = record->receivedBytes;
    g_sotaUpdateManager.programmedBytes = record->programmedBytes;
    g_sotaUpdateManager.verifiedBytes = record->verifiedBytes;
    g_sotaUpdateManager.expectedSeq = record->lastSeq + 1U;
    g_sotaUpdateManager.expectedOffset = record->receivedBytes;
    g_sotaUpdateManager.lastSeq = record->lastSeq;
    g_sotaUpdateManager.lastOffset = record->lastOffset;
    g_sotaUpdateManager.lastError = record->lastError;
    g_sotaUpdateManager.rollbackReason = record->rollbackReason;
}

static boolean SotaUpdateManager_IsStartAllowed(void)
{
    boolean allowed = FALSE;

    switch (g_sotaUpdateManager.state)
    {
    case SOTA_UPDATE_MANAGER_STATE_IDLE:
    case SOTA_UPDATE_MANAGER_STATE_ABORTED:
    case SOTA_UPDATE_MANAGER_STATE_COMMITTED:
    case SOTA_UPDATE_MANAGER_STATE_ROLLED_BACK:
    case SOTA_UPDATE_MANAGER_STATE_ERROR:
        allowed = TRUE;
        break;

    default:
        allowed = FALSE;
        break;
    }

    return allowed;
}

static boolean SotaUpdateManager_IsAbortAllowed(void)
{
    boolean allowed = FALSE;

    switch (g_sotaUpdateManager.state)
    {
    case SOTA_UPDATE_MANAGER_STATE_SESSION_OPEN:
    case SOTA_UPDATE_MANAGER_STATE_DOWNLOAD_REQUESTED:
    case SOTA_UPDATE_MANAGER_STATE_ERASE_PENDING:
    case SOTA_UPDATE_MANAGER_STATE_ERASING:
    case SOTA_UPDATE_MANAGER_STATE_RECEIVING:
    case SOTA_UPDATE_MANAGER_STATE_WRITE_PENDING:
    case SOTA_UPDATE_MANAGER_STATE_PROGRAMMING:
    case SOTA_UPDATE_MANAGER_STATE_TRANSFER_EXIT_RECEIVED:
    case SOTA_UPDATE_MANAGER_STATE_VERIFY_PENDING:
    case SOTA_UPDATE_MANAGER_STATE_VERIFYING:
    case SOTA_UPDATE_MANAGER_STATE_VERIFIED:
        allowed = TRUE;
        break;

    default:
        allowed = FALSE;
        break;
    }

    return allowed;
}

static boolean SotaUpdateManager_IsBusyState(SotaUpdateManager_State_t state)
{
    boolean busy = FALSE;

    switch (state)
    {
    case SOTA_UPDATE_MANAGER_STATE_DOWNLOAD_REQUESTED:
    case SOTA_UPDATE_MANAGER_STATE_ERASE_PENDING:
    case SOTA_UPDATE_MANAGER_STATE_ERASING:
    case SOTA_UPDATE_MANAGER_STATE_WRITE_PENDING:
    case SOTA_UPDATE_MANAGER_STATE_PROGRAMMING:
    case SOTA_UPDATE_MANAGER_STATE_VERIFY_PENDING:
    case SOTA_UPDATE_MANAGER_STATE_VERIFYING:
    case SOTA_UPDATE_MANAGER_STATE_SWAP_ARM_REQUESTED:
    case SOTA_UPDATE_MANAGER_STATE_ROLLBACK_REQUESTED:
        busy = TRUE;
        break;

    default:
        busy = FALSE;
        break;
    }

    return busy;
}

static boolean SotaUpdateManager_IsRollbackAllowedFromError(void)
{
    SotaMetadata_Record_t record;
    uint8 currentBank;

    if (SotaMetadata_LoadLatest(&record) != SOTA_METADATA_STATUS_OK)
    {
        return FALSE;
    }

    if (record.state == SOTA_METADATA_STATE_TESTING_AFTER_BOOT)
    {
        return TRUE;
    }

    currentBank = (uint8)SotaSwapManager_GetCurrentBank();
    if ((SotaUpdateManager_IsKnownBank(currentBank) == TRUE) &&
        (SotaUpdateManager_IsKnownBank(record.targetBank) == TRUE) &&
        (currentBank == record.targetBank) &&
        (record.activeBankBeforeUpdate != currentBank))
    {
        return TRUE;
    }

    return FALSE;
}

static boolean SotaUpdateManager_IsKnownBank(uint8 bank)
{
    return ((bank == (uint8)SOTA_TC37X_PFLASH_BANK_PF0) ||
            (bank == (uint8)SOTA_TC37X_PFLASH_BANK_PF1)) ? TRUE : FALSE;
}

static uint8 SotaUpdateManager_GetRollbackTargetBank(void)
{
    uint8 currentBank = (uint8)SotaSwapManager_GetCurrentBank();
    uint8 rollbackBank = g_sotaUpdateManager.activeBankBeforeUpdate;

    if ((SotaUpdateManager_IsKnownBank(rollbackBank) == FALSE) ||
        (rollbackBank == currentBank))
    {
        rollbackBank = (uint8)SotaSwapManager_GetInactiveBank();
    }

    if ((SotaUpdateManager_IsKnownBank(rollbackBank) == FALSE) ||
        (rollbackBank == currentBank))
    {
        rollbackBank = SOTA_UPDATE_MANAGER_INVALID_BANK;
    }

    return rollbackBank;
}

static SotaCanFd_ResponseCode_t SotaUpdateManager_ValidateBegin(const SotaCanFd_BeginRequest_t *request)
{
    SotaImage_Manifest_t manifest;

    if (request == NULL_PTR)
    {
        return SOTA_RSP_INVALID_PARAM;
    }

    SotaUpdateManager_BuildManifest(request, &manifest);
    return SotaUpdateManager_MapImageStatus(
        SotaImage_ValidateManifest(&manifest));
}

static void SotaUpdateManager_BuildManifest(const SotaCanFd_BeginRequest_t *request,
                                            SotaImage_Manifest_t *manifest)
{
    if ((request == NULL_PTR) || (manifest == NULL_PTR))
    {
        return;
    }

    (void)memset(manifest, 0, sizeof(SotaImage_Manifest_t));
    manifest->magic = SOTA_IMAGE_MANIFEST_MAGIC;
    manifest->protocolVersion = request->protocolVersion;
    manifest->ecuType = (uint8)request->ecuType;
    manifest->imageVersion = request->imageVersion;
    manifest->imageSize = request->imageSize;
    manifest->imageCrc = request->imageCrc;
    manifest->manifestCrc = request->manifestCrc;
    manifest->flags = request->flags;
}

static SotaCanFd_ResponseCode_t SotaUpdateManager_MapImageStatus(SotaImage_Status_t status)
{
    SotaCanFd_ResponseCode_t responseCode;

    switch (status)
    {
    case SOTA_IMAGE_STATUS_OK:
        responseCode = SOTA_RSP_OK;
        break;

    case SOTA_IMAGE_STATUS_ECU_MISMATCH:
        responseCode = SOTA_RSP_AUTH_ERROR;
        break;

    case SOTA_IMAGE_STATUS_CRC_ERROR:
        responseCode = SOTA_RSP_CRC_ERROR;
        break;

    case SOTA_IMAGE_STATUS_INVALID_SIZE:
        responseCode = SOTA_RSP_INVALID_OFFSET;
        break;

    case SOTA_IMAGE_STATUS_INVALID_PARAM:
    case SOTA_IMAGE_STATUS_INVALID_MAGIC:
    case SOTA_IMAGE_STATUS_UNSUPPORTED_VERSION:
    default:
        responseCode = SOTA_RSP_INVALID_PARAM;
        break;
    }

    return responseCode;
}

static boolean SotaUpdateManager_IsValidData(const SotaCanFd_DataRequest_t *request)
{
    if (request == NULL_PTR)
    {
        return FALSE;
    }

    if ((request->payloadLen == 0U) ||
        (request->payloadLen > SOTA_CANFD_DATA_MAX_PAYLOAD_SIZE))
    {
        return FALSE;
    }

    if (request->seq != g_sotaUpdateManager.expectedSeq)
    {
        return FALSE;
    }

    if (request->offset < g_sotaUpdateManager.expectedOffset)
    {
        return FALSE;
    }

    if (request->offset > g_sotaUpdateManager.imageSize)
    {
        return FALSE;
    }

    if ((uint32)request->payloadLen > (g_sotaUpdateManager.imageSize - request->offset))
    {
        return FALSE;
    }

    return TRUE;
}

static void SotaUpdateManager_ResetContext(void)
{
    (void)memset(&g_sotaUpdateManager, 0, sizeof(g_sotaUpdateManager));
    g_sotaUpdateManager.state = SOTA_UPDATE_MANAGER_STATE_IDLE;
    g_sotaUpdateManager.ecuType = SOTA_ECU_TYPE_UNKNOWN;
    g_sotaUpdateManager.activeBankBeforeUpdate = SOTA_UPDATE_MANAGER_INVALID_BANK;
    g_sotaUpdateManager.targetBank = SOTA_UPDATE_MANAGER_INVALID_BANK;
    g_metadataStatePending = FALSE;
    g_metadataProgressPending = FALSE;
    g_metadataErrorPending = FALSE;
    g_pendingMetadataState = SOTA_METADATA_STATE_IDLE;
}

static void SotaUpdateManager_SetState(SotaUpdateManager_State_t state)
{
    SotaMetadata_State_t metadataState;

    if (g_sotaUpdateManager.state != state)
    {
        g_sotaUpdateManager.state = state;
        if (SotaUpdateManager_MapMetadataState(state, &metadataState) == TRUE)
        {
            SotaUpdateManager_RequestMetadataState(metadataState);
        }
    }
}

static void SotaUpdateManager_SetRuntimeState(SotaUpdateManager_State_t state)
{
    g_sotaUpdateManager.state = state;
}

static void SotaUpdateManager_EnterError(SotaCanFd_ResponseCode_t errorCode)
{
    SotaUpdateManager_EnterErrorWithCode(errorCode, (uint32)errorCode);
}

static void SotaUpdateManager_EnterErrorWithCode(SotaCanFd_ResponseCode_t responseCode,
                                                 uint32 detailCode)
{
    (void)responseCode;
    g_sotaUpdateManager.lastError = detailCode;
    g_sotaUpdateManager.state = SOTA_UPDATE_MANAGER_STATE_ERROR;
    g_metadataStatePending = FALSE;
    g_metadataErrorPending = TRUE;
}

static void SotaUpdateManager_RequestMetadataState(SotaMetadata_State_t state)
{
    g_pendingMetadataState = state;
    g_metadataStatePending = TRUE;
}

static void SotaUpdateManager_RequestMetadataProgress(void)
{
    g_metadataProgressPending = TRUE;
}

static void SotaUpdateManager_FlushMetadata(void)
{
    SotaMetadata_Status_t metadataStatus;
    SotaMetadata_State_t snapshotState;
    boolean writeSnapshot = FALSE;

    /*
     * Metadata writes program DFlash, so they are flushed from RunStep()
     * instead of the CAN-FD RX handler path.
     */
    if (g_metadataStatePending == TRUE)
    {
        snapshotState = g_pendingMetadataState;
        writeSnapshot = TRUE;
        g_metadataStatePending = FALSE;
    }
    else if (SotaUpdateManager_MapMetadataState(g_sotaUpdateManager.state,
                                                &snapshotState) == FALSE)
    {
        snapshotState = SOTA_METADATA_STATE_IDLE;
    }

    if (g_metadataErrorPending == TRUE)
    {
        snapshotState = SOTA_METADATA_STATE_ERROR;
        writeSnapshot = TRUE;
        g_metadataErrorPending = FALSE;
    }

    if (g_metadataProgressPending == TRUE)
    {
        writeSnapshot = TRUE;
        g_metadataProgressPending = FALSE;
    }

    if (writeSnapshot == TRUE)
    {
        metadataStatus = SotaUpdateManager_WriteMetadataSnapshot(snapshotState);
        if (metadataStatus != SOTA_METADATA_STATUS_OK)
        {
            g_sotaUpdateManager.lastError =
                SotaUpdateManager_MetadataError(metadataStatus);
            g_sotaUpdateManager.state = SOTA_UPDATE_MANAGER_STATE_ERROR;
        }
    }
}

static boolean SotaUpdateManager_MapMetadataState(SotaUpdateManager_State_t updateState,
                                                  SotaMetadata_State_t *outMetadataState)
{
    boolean mapped = TRUE;

    if (outMetadataState == NULL_PTR)
    {
        return FALSE;
    }

    switch (updateState)
    {
    case SOTA_UPDATE_MANAGER_STATE_IDLE:
        *outMetadataState = SOTA_METADATA_STATE_IDLE;
        break;

    case SOTA_UPDATE_MANAGER_STATE_SESSION_OPEN:
    case SOTA_UPDATE_MANAGER_STATE_DOWNLOAD_REQUESTED:
        *outMetadataState = SOTA_METADATA_STATE_DOWNLOAD_REQUESTED;
        break;

    case SOTA_UPDATE_MANAGER_STATE_ERASE_PENDING:
    case SOTA_UPDATE_MANAGER_STATE_ERASING:
        *outMetadataState = SOTA_METADATA_STATE_ERASING;
        break;

    case SOTA_UPDATE_MANAGER_STATE_RECEIVING:
    case SOTA_UPDATE_MANAGER_STATE_TRANSFER_EXIT_RECEIVED:
        *outMetadataState = SOTA_METADATA_STATE_RECEIVING;
        break;

    case SOTA_UPDATE_MANAGER_STATE_WRITE_PENDING:
    case SOTA_UPDATE_MANAGER_STATE_PROGRAMMING:
        *outMetadataState = SOTA_METADATA_STATE_PROGRAMMING;
        break;

    case SOTA_UPDATE_MANAGER_STATE_VERIFY_PENDING:
    case SOTA_UPDATE_MANAGER_STATE_VERIFYING:
        *outMetadataState = SOTA_METADATA_STATE_VERIFYING;
        break;

    case SOTA_UPDATE_MANAGER_STATE_VERIFIED:
        *outMetadataState = SOTA_METADATA_STATE_VERIFIED;
        break;

    case SOTA_UPDATE_MANAGER_STATE_SWAP_ARMED:
        *outMetadataState = SOTA_METADATA_STATE_SWAP_ARMED;
        break;

    case SOTA_UPDATE_MANAGER_STATE_SWAP_ARM_REQUESTED:
        *outMetadataState = SOTA_METADATA_STATE_SWAP_ARM_REQUESTED;
        break;

    case SOTA_UPDATE_MANAGER_STATE_TESTING_AFTER_BOOT:
        *outMetadataState = SOTA_METADATA_STATE_TESTING_AFTER_BOOT;
        break;

    case SOTA_UPDATE_MANAGER_STATE_COMMITTED:
        *outMetadataState = SOTA_METADATA_STATE_COMMITTED;
        break;

    case SOTA_UPDATE_MANAGER_STATE_ROLLBACK_ARMED:
        *outMetadataState = SOTA_METADATA_STATE_ROLLBACK_ARMED;
        break;

    case SOTA_UPDATE_MANAGER_STATE_ROLLBACK_REQUESTED:
        *outMetadataState = SOTA_METADATA_STATE_ROLLBACK_REQUESTED;
        break;

    case SOTA_UPDATE_MANAGER_STATE_ROLLED_BACK:
        *outMetadataState = SOTA_METADATA_STATE_ROLLED_BACK;
        break;

    case SOTA_UPDATE_MANAGER_STATE_ERROR:
        *outMetadataState = SOTA_METADATA_STATE_ERROR;
        break;

    case SOTA_UPDATE_MANAGER_STATE_ABORTED:
    default:
        mapped = FALSE;
        break;
    }

    return mapped;
}

static uint32 SotaUpdateManager_MetadataError(SotaMetadata_Status_t status)
{
    return SOTA_UPDATE_MANAGER_METADATA_ERROR_BASE | (uint32)status;
}

static SotaMetadata_Status_t SotaUpdateManager_WriteMetadataSnapshot(SotaMetadata_State_t state)
{
    SotaMetadata_Record_t record;
    SotaMetadata_Status_t status;
    uint16 swapEntryIndex;

    status = SotaMetadata_LoadLatest(&record);
    if (status != SOTA_METADATA_STATUS_OK)
    {
        if ((status != SOTA_METADATA_STATUS_EMPTY) &&
            (status != SOTA_METADATA_STATUS_NOT_FOUND))
        {
            return status;
        }

        SotaUpdateManager_SetDefaultMetadataRecord(&record);
    }

    record.state = state;
    record.ecuType = (uint16)g_sotaUpdateManager.ecuType;
    record.activeBankBeforeUpdate = g_sotaUpdateManager.activeBankBeforeUpdate;
    record.targetBank = g_sotaUpdateManager.targetBank;
    record.currentActiveBank = (uint8)SotaSwapManager_GetCurrentBank();
    record.imageVersion = g_sotaUpdateManager.imageVersion;
    record.imageSize = g_sotaUpdateManager.imageSize;
    record.imageCrc = g_sotaUpdateManager.imageCrc;
    record.manifestCrc = g_sotaUpdateManager.manifestCrc;
    record.receivedBytes = g_sotaUpdateManager.receivedBytes;
    record.programmedBytes = g_sotaUpdateManager.programmedBytes;
    record.verifiedBytes = g_sotaUpdateManager.verifiedBytes;
    record.lastSeq = g_sotaUpdateManager.lastSeq;
    record.lastOffset = g_sotaUpdateManager.lastOffset;
    record.lastError = g_sotaUpdateManager.lastError;
    record.rollbackReason = g_sotaUpdateManager.rollbackReason;

    SotaUpdateManager_ResetTrialMetadataForNewUpdate(&record, state);

    swapEntryIndex = SotaSwapManager_GetLastSwapEntryIndex();
    if (swapEntryIndex != 0xFFFFU)
    {
        record.swapEntryIndex = swapEntryIndex;
    }

    if (record.maxBootAttempt == 0U)
    {
        record.maxBootAttempt = 1U;
    }

    return SotaMetadata_Append(&record);
}

static void SotaUpdateManager_SetDefaultMetadataRecord(SotaMetadata_Record_t *record)
{
    if (record != NULL_PTR)
    {
        (void)memset(record, 0, sizeof(SotaMetadata_Record_t));
        record->state = SOTA_METADATA_STATE_IDLE;
        record->activeBankBeforeUpdate = SOTA_UPDATE_MANAGER_INVALID_BANK;
        record->targetBank = SOTA_UPDATE_MANAGER_INVALID_BANK;
        record->currentActiveBank = SOTA_UPDATE_MANAGER_INVALID_BANK;
        record->swapEntryIndex = 0xFFFFU;
        record->maxBootAttempt = 1U;
    }
}

static void SotaUpdateManager_ResetTrialMetadataForNewUpdate(SotaMetadata_Record_t *record,
                                                            SotaMetadata_State_t state)
{
    if (record == NULL_PTR)
    {
        return;
    }

    if (state == SOTA_METADATA_STATE_DOWNLOAD_REQUESTED)
    {
        record->bootAttempt = 0U;
        record->swapEntryIndex = 0xFFFFU;
    }
}

static boolean SotaUpdateManager_MapRuntimeState(SotaMetadata_State_t metadataState,
                                                 SotaUpdateManager_State_t *outRuntimeState)
{
    if (outRuntimeState == NULL_PTR)
    {
        return FALSE;
    }

    switch (metadataState)
    {
    case SOTA_METADATA_STATE_IDLE:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_IDLE;
        break;

    case SOTA_METADATA_STATE_DOWNLOAD_REQUESTED:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_DOWNLOAD_REQUESTED;
        break;

    case SOTA_METADATA_STATE_ERASING:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_ERASING;
        break;

    case SOTA_METADATA_STATE_RECEIVING:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_RECEIVING;
        break;

    case SOTA_METADATA_STATE_PROGRAMMING:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_PROGRAMMING;
        break;

    case SOTA_METADATA_STATE_VERIFYING:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_VERIFYING;
        break;

    case SOTA_METADATA_STATE_VERIFIED:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_VERIFIED;
        break;

    case SOTA_METADATA_STATE_SWAP_ARMED:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_SWAP_ARMED;
        break;

    case SOTA_METADATA_STATE_SWAP_ARM_REQUESTED:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_SWAP_ARM_REQUESTED;
        break;

    case SOTA_METADATA_STATE_TESTING_AFTER_BOOT:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_TESTING_AFTER_BOOT;
        break;

    case SOTA_METADATA_STATE_COMMITTED:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_COMMITTED;
        break;

    case SOTA_METADATA_STATE_ROLLBACK_ARMED:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_ROLLBACK_ARMED;
        break;

    case SOTA_METADATA_STATE_ROLLBACK_REQUESTED:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_ROLLBACK_REQUESTED;
        break;

    case SOTA_METADATA_STATE_ROLLED_BACK:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_ROLLED_BACK;
        break;

    case SOTA_METADATA_STATE_ERROR:
        *outRuntimeState = SOTA_UPDATE_MANAGER_STATE_ERROR;
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

static SotaCanFd_ResponseCode_t SotaUpdateManager_MapCoreResult(SotaUpdateCore_Result_t result)
{
    SotaCanFd_ResponseCode_t responseCode;

    switch (result)
    {
    case SOTA_UPDATECORE_RESULT_OK:
        responseCode = SOTA_RSP_OK;
        break;

    case SOTA_UPDATECORE_RESULT_PENDING:
        responseCode = SOTA_RSP_PENDING;
        break;

    case SOTA_UPDATECORE_RESULT_INVALID_STATE:
        responseCode = SOTA_RSP_INVALID_STATE;
        break;

    case SOTA_UPDATECORE_RESULT_INVALID_PARAM:
        responseCode = SOTA_RSP_INVALID_PARAM;
        break;

    case SOTA_UPDATECORE_RESULT_INVALID_RANGE:
        responseCode = SOTA_RSP_INVALID_OFFSET;
        break;

    case SOTA_UPDATECORE_RESULT_CRC_ERROR:
        responseCode = SOTA_RSP_CRC_ERROR;
        break;

    case SOTA_UPDATECORE_RESULT_FLASH_ERROR:
        responseCode = SOTA_RSP_FLASH_ERROR;
        break;

    case SOTA_UPDATECORE_RESULT_VERIFY_ERROR:
        responseCode = SOTA_RSP_VERIFY_ERROR;
        break;

    case SOTA_UPDATECORE_RESULT_INTERNAL_ERROR:
    default:
        responseCode = SOTA_RSP_INTERNAL_ERROR;
        break;
    }

    return responseCode;
}

static SotaCanFd_ResponseCode_t SotaUpdateManager_MapSwapResult(SotaSwapManager_Result_t result)
{
    SotaCanFd_ResponseCode_t responseCode;

    switch (result)
    {
    case SOTA_SWAP_MANAGER_RESULT_OK:
        responseCode = SOTA_RSP_OK;
        break;

    case SOTA_SWAP_MANAGER_RESULT_INVALID_PARAM:
        responseCode = SOTA_RSP_INVALID_PARAM;
        break;

    case SOTA_SWAP_MANAGER_RESULT_INVALID_STATE:
        responseCode = SOTA_RSP_INVALID_STATE;
        break;

    case SOTA_SWAP_MANAGER_RESULT_NO_FREE_ENTRY:
    case SOTA_SWAP_MANAGER_RESULT_NOT_CONFIGURED:
    case SOTA_SWAP_MANAGER_RESULT_WRITE_ERROR:
    case SOTA_SWAP_MANAGER_RESULT_VERIFY_ERROR:
    default:
        responseCode = SOTA_RSP_SWAP_ERROR;
        break;
    }

    return responseCode;
}

static uint32 SotaUpdateManager_GetSwapErrorCode(SotaSwapManager_Result_t result)
{
    return (result == SOTA_SWAP_MANAGER_RESULT_NO_FREE_ENTRY) ?
           SOTA_UPDATE_ERROR_SWAP_LOG_FULL :
           SOTA_UPDATE_ERROR_SWAP_ARM_FAILED;
}

static void SotaUpdateManager_RefreshCoreProgress(void)
{
    SotaUpdateCore_Progress_t progress;

    SotaUpdateCore_GetProgress(&progress);

    if (progress.targetBank != SOTA_UPDATE_MANAGER_INVALID_BANK)
    {
        g_sotaUpdateManager.targetBank = progress.targetBank;
    }

    if (progress.imageSize != 0U)
    {
        if (progress.receivedBytes > g_sotaUpdateManager.receivedBytes)
        {
            g_sotaUpdateManager.receivedBytes = progress.receivedBytes;
        }

        g_sotaUpdateManager.programmedBytes = progress.programmedBytes;
        g_sotaUpdateManager.verifiedBytes = progress.verifiedBytes;
    }

    if (progress.lastError != 0U)
    {
        g_sotaUpdateManager.lastError = progress.lastError;
    }
}

static SotaCanFd_ResponseCode_t SotaUpdateManager_ProcessPendingChunk(void)
{
    SotaUpdateCore_Result_t coreResult;
    SotaCanFd_ResponseCode_t result;

    if (g_sotaUpdateManager.pendingChunkValid == FALSE)
    {
        SotaUpdateManager_EnterError(SOTA_RSP_INTERNAL_ERROR);
        return SOTA_RSP_INTERNAL_ERROR;
    }

    coreResult =
        SotaUpdateCore_WriteChunk(g_sotaUpdateManager.pendingChunk.offset,
                                  g_sotaUpdateManager.pendingChunk.payload,
                                  (uint32)g_sotaUpdateManager.pendingChunk.payloadLen,
                                  g_sotaUpdateManager.pendingChunk.chunkCrc);
    SotaUpdateManager_RefreshCoreProgress();
    result = SotaUpdateManager_MapCoreResult(coreResult);

    if (result == SOTA_RSP_OK)
    {
        g_sotaUpdateManager.lastSeq = g_sotaUpdateManager.pendingChunk.seq;
        g_sotaUpdateManager.lastOffset = g_sotaUpdateManager.pendingChunk.offset;
        g_sotaUpdateManager.expectedSeq++;
        g_sotaUpdateManager.expectedOffset = g_sotaUpdateManager.receivedBytes;
        g_sotaUpdateManager.pendingChunkValid = FALSE;
    }

    return result;
}

static SotaCanFd_ResponseCode_t SotaUpdateManager_RunVerifyStep(void)
{
    SotaUpdateCore_Result_t coreResult;
    SotaCanFd_ResponseCode_t result;

    if (g_sotaUpdateManager.state == SOTA_UPDATE_MANAGER_STATE_VERIFY_PENDING)
    {
        coreResult = SotaUpdateCore_Finalize();
        SotaUpdateManager_RefreshCoreProgress();
        result = SotaUpdateManager_MapCoreResult(coreResult);
        if (result != SOTA_RSP_OK)
        {
            return result;
        }

        SotaUpdateManager_RequestMetadataProgress();
        SotaUpdateManager_SetState(SOTA_UPDATE_MANAGER_STATE_VERIFYING);
    }

    coreResult = SotaUpdateCore_VerifyStep();
    SotaUpdateManager_RefreshCoreProgress();
    result = SotaUpdateManager_MapCoreResult(coreResult);

    if (result == SOTA_RSP_OK)
    {
        SotaUpdateManager_RequestMetadataProgress();
    }

    return result;
}
