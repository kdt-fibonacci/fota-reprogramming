/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include <string.h>

#include "FotaHandler.h"
#include "Update/Sota_UpdateCore.h"
#include "Swap/Sota_SwapDiag.h"

#ifndef NULL_PTR
#define NULL_PTR ((void *)0)
#endif

/*********************************************************************************************************************/
/*------------------------------------------------------Types--------------------------------------------------------*/
/*********************************************************************************************************************/

typedef struct
{
    FotaHandlerStateType handlerState;
    FotaHandlerResultType lastHandlerResult;

    FotaChunkStateType chunkState;
    uint8 buffer[FOTA_MAX_TRANSFER_DATA_LENGTH];
    uint32 length;
    uint8 blockSequenceCounter;

    uint32 expectedImageLength;
    uint32 expectedImageCrc;
    uint32 totalReceivedBytes;

    uint8 initialized;
    uint8 downloadStarted;
    uint8 verified;
    uint8 activationArmed;

    SotaUpdateResult_t lastUpdateResult;
    SotaProvision_Result lastProvisionResult;
    uint32 lastSwapEntryIndex;
    uint32 lastSwapTargetModeWord;
} FotaHandlerContextType;

/*********************************************************************************************************************/
/*-------------------------------------------------Static Variables--------------------------------------------------*/
/*********************************************************************************************************************/

static FotaHandlerContextType g_fotaHandlerContext;

/*********************************************************************************************************************/
/*-----------------------------------------------Private Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static void FOTA_ClearChunkOnly(void)
{
    g_fotaHandlerContext.length = 0U;
    g_fotaHandlerContext.blockSequenceCounter = 0U;
}

static void FOTA_SetDefaultsAfterMemset(void)
{
    g_fotaHandlerContext.initialized = 1U;
    g_fotaHandlerContext.handlerState = FOTA_HANDLER_STATE_IDLE;
    g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_NONE;
    g_fotaHandlerContext.chunkState = FOTA_CHUNK_IDLE;
    g_fotaHandlerContext.lastUpdateResult = SOTA_UPDATE_OK;
    g_fotaHandlerContext.lastProvisionResult = SOTA_PROVISION_OK;
    g_fotaHandlerContext.lastSwapEntryIndex = 0xFFFFFFFFu;
    g_fotaHandlerContext.lastSwapTargetModeWord = 0U;
}

static void FOTA_SetHandlerError(FotaHandlerResultType handlerResult,
                                 SotaUpdateResult_t updateResult)
{
    g_fotaHandlerContext.lastHandlerResult = handlerResult;
    g_fotaHandlerContext.lastUpdateResult = updateResult;
    g_fotaHandlerContext.chunkState = FOTA_CHUNK_ERROR;
    g_fotaHandlerContext.handlerState = FOTA_HANDLER_STATE_ERROR;
}

static Std_ReturnType FOTA_MapUpdateResult(SotaUpdateResult_t result,
                                           FotaHandlerResultType failResult)
{
    g_fotaHandlerContext.lastUpdateResult = result;

    if (result == SOTA_UPDATE_OK)
    {
        g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_OK;
        return E_OK;
    }

    g_fotaHandlerContext.lastHandlerResult = failResult;
    g_fotaHandlerContext.handlerState = FOTA_HANDLER_STATE_ERROR;
    return E_NOT_OK;
}

static Std_ReturnType FOTA_MapProvisionResult(SotaProvision_Result result,
                                              FotaHandlerResultType failResult)
{
    g_fotaHandlerContext.lastProvisionResult = result;

    if (result == SOTA_PROVISION_OK)
    {
        g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_OK;
        return E_OK;
    }

    g_fotaHandlerContext.lastHandlerResult = failResult;
    g_fotaHandlerContext.handlerState = FOTA_HANDLER_STATE_ERROR;
    return E_NOT_OK;
}

static uint8 FOTA_HasActiveChunk(void)
{
    return (uint8)((g_fotaHandlerContext.chunkState == FOTA_CHUNK_RECEIVED) ||
                   (g_fotaHandlerContext.chunkState == FOTA_CHUNK_PROCESSING) ||
                   (g_fotaHandlerContext.chunkState == FOTA_CHUNK_DONE));
}

/*********************************************************************************************************************/
/*------------------------------------------------Public Functions---------------------------------------------------*/
/*********************************************************************************************************************/

void FOTA_Init(void)
{
    FOTA_ResetContext();
}

void FOTA_ResetContext(void)
{
    (void)memset(&g_fotaHandlerContext, 0, sizeof(g_fotaHandlerContext));
    FOTA_SetDefaultsAfterMemset();

    /* Reset the existing working Reprogram core software state only. */
    SotaUpdate_Reset();
}

Std_ReturnType FOTA_ProvisionInitialOnce(void)
{
    SotaProvision_Result result;

    if (g_fotaHandlerContext.initialized == 0U)
    {
        FOTA_Init();
    }

    /*
     * This may write OTP/UCB depending on current target state and configuration.
     * It is intentionally explicit and is not called by FOTA_Init().
     */
    result = SotaProvision_RunInitialOnce();
    return FOTA_MapProvisionResult(result, FOTA_HANDLER_RESULT_PROVISION_FAILED);
}

Std_ReturnType FOTA_RunInitialProvisioningOnce(void)
{
    return FOTA_ProvisionInitialOnce();
}

Std_ReturnType FOTA_StartDownload(uint32 imageLength)
{
    SotaUpdateResult_t result;

    if (imageLength == 0U)
    {
        g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_INVALID_PARAM;
        g_fotaHandlerContext.handlerState = FOTA_HANDLER_STATE_ERROR;
        return E_NOT_OK;
    }

    /* A new RequestDownload starts a new update attempt. */
    FOTA_ResetContext();

    result = SotaUpdate_Begin(imageLength);
    g_fotaHandlerContext.lastUpdateResult = result;

    if (result != SOTA_UPDATE_OK)
    {
        g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_UPDATE_FAILED;
        g_fotaHandlerContext.handlerState = FOTA_HANDLER_STATE_ERROR;
        return E_NOT_OK;
    }

    g_fotaHandlerContext.expectedImageLength = imageLength;
    g_fotaHandlerContext.expectedImageCrc = 0U;
    g_fotaHandlerContext.totalReceivedBytes = 0U;
    g_fotaHandlerContext.downloadStarted = 1U;
    g_fotaHandlerContext.verified = 0U;
    g_fotaHandlerContext.activationArmed = 0U;
    g_fotaHandlerContext.handlerState = FOTA_HANDLER_STATE_DOWNLOAD_STARTED;
    g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_OK;

    return E_OK;
}

Dcm_ReturnWriteMemoryType FOTA_ProcessTransferDataWrite(
    Dcm_OpStatusType OpStatus,
    const uint8 *DataPtr,
    uint32 DataLength,
    uint8 BlockSequenceCounter
)
{
    if (g_fotaHandlerContext.initialized == 0U)
    {
        FOTA_Init();
    }

    if (g_fotaHandlerContext.downloadStarted == 0U)
    {
        g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_INVALID_STATE;
        return DCM_WRITE_FAILED;
    }

    if (OpStatus == DCM_OP_INITIAL)
    {
        if ((DataPtr == NULL_PTR) ||
            (DataLength == 0U) ||
            (DataLength > FOTA_MAX_TRANSFER_DATA_LENGTH))
        {
            FOTA_SetHandlerError(FOTA_HANDLER_RESULT_INVALID_PARAM, SOTA_UPDATE_INVALID_PARAM);
            return DCM_WRITE_FAILED;
        }

        if (g_fotaHandlerContext.chunkState == FOTA_CHUNK_IDLE)
        {
            (void)memcpy(&g_fotaHandlerContext.buffer[0], DataPtr, DataLength);
            g_fotaHandlerContext.length = DataLength;
            g_fotaHandlerContext.blockSequenceCounter = BlockSequenceCounter;
            g_fotaHandlerContext.lastUpdateResult = SOTA_UPDATE_OK;
            g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_BUSY;
            g_fotaHandlerContext.chunkState = FOTA_CHUNK_RECEIVED;
            g_fotaHandlerContext.handlerState = FOTA_HANDLER_STATE_TRANSFERRING;

            return DCM_WRITE_PENDING;
        }

        /* Do not overwrite a chunk still owned by the previous pending DCM transaction. */
        if ((g_fotaHandlerContext.chunkState == FOTA_CHUNK_RECEIVED) ||
            (g_fotaHandlerContext.chunkState == FOTA_CHUNK_PROCESSING))
        {
            g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_BUSY;
            return DCM_WRITE_PENDING;
        }

        if (g_fotaHandlerContext.chunkState == FOTA_CHUNK_DONE)
        {
            /* Previous pending operation finished but DCM has not consumed DCM_OP_PENDING yet. */
            g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_BUSY;
            return DCM_WRITE_PENDING;
        }

        return DCM_WRITE_FAILED;
    }

    if (OpStatus == DCM_OP_PENDING)
    {
        if (g_fotaHandlerContext.chunkState == FOTA_CHUNK_DONE)
        {
            g_fotaHandlerContext.chunkState = FOTA_CHUNK_IDLE;
            FOTA_ClearChunkOnly();
            g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_OK;
            return DCM_WRITE_OK;
        }

        if (g_fotaHandlerContext.chunkState == FOTA_CHUNK_ERROR)
        {
            return DCM_WRITE_FAILED;
        }

        if ((g_fotaHandlerContext.chunkState == FOTA_CHUNK_RECEIVED) ||
            (g_fotaHandlerContext.chunkState == FOTA_CHUNK_PROCESSING))
        {
            return DCM_WRITE_PENDING;
        }

        /* Pending without an active chunk means DCM call sequencing is wrong. */
        g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_INVALID_STATE;
        return DCM_WRITE_FAILED;
    }

    g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_INVALID_PARAM;
    return DCM_WRITE_FAILED;
}

void FOTAHandlerMain(void)
{
    SotaUpdateResult_t result;

    if (g_fotaHandlerContext.chunkState != FOTA_CHUNK_RECEIVED)
    {
        return;
    }

    g_fotaHandlerContext.chunkState = FOTA_CHUNK_PROCESSING;

    result = SotaUpdate_WriteChunk(&g_fotaHandlerContext.buffer[0],
                                   g_fotaHandlerContext.length);
    g_fotaHandlerContext.lastUpdateResult = result;

    if (result == SOTA_UPDATE_OK)
    {
        g_fotaHandlerContext.totalReceivedBytes += g_fotaHandlerContext.length;
        g_fotaHandlerContext.chunkState = FOTA_CHUNK_DONE;
        g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_OK;
    }
    else
    {
        FOTA_SetHandlerError(FOTA_HANDLER_RESULT_UPDATE_FAILED, result);
    }
}

Std_ReturnType FOTA_RequestTransferExit(uint32 expectedCrc)
{
    SotaUpdateResult_t result;

    if (g_fotaHandlerContext.downloadStarted == 0U)
    {
        g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_INVALID_STATE;
        return E_NOT_OK;
    }

    if (FOTA_HasActiveChunk() != 0U)
    {
        g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_BUSY;
        return E_NOT_OK;
    }

    g_fotaHandlerContext.expectedImageCrc = expectedCrc;

    if (g_fotaHandlerContext.verified != 0U)
    {
        return E_OK;
    }

    result = SotaUpdate_FinalizeAndVerify(expectedCrc);

    if (FOTA_MapUpdateResult(result, FOTA_HANDLER_RESULT_VERIFY_FAILED) != E_OK)
    {
        return E_NOT_OK;
    }

    g_fotaHandlerContext.verified = 1U;
    g_fotaHandlerContext.handlerState = FOTA_HANDLER_STATE_VERIFIED;
    return E_OK;
}

Std_ReturnType FOTA_VerifyImage(uint32 expectedCrc)
{
    return FOTA_RequestTransferExit(expectedCrc);
}

Std_ReturnType FOTA_ActivateImage(void)
{
    SotaProvision_Result result;
    uint32 entryIndex;
    uint32 targetModeWord;

#if (FOTA_ALLOW_ACTIVATE_WITHOUT_VERIFY == 0U)
    if (g_fotaHandlerContext.verified == 0U)
    {
        g_fotaHandlerContext.lastHandlerResult = FOTA_HANDLER_RESULT_INVALID_STATE;
        return E_NOT_OK;
    }
#endif

    entryIndex = 0xFFFFFFFFu;
    targetModeWord = 0U;

    /*
     * Arms next UCB_SWAP entry only.
     * Does not jump and does not reset.
     */
    result = SotaProvision_ProgramNextSwapEntry(&entryIndex, &targetModeWord);

    g_fotaHandlerContext.lastSwapEntryIndex = entryIndex;
    g_fotaHandlerContext.lastSwapTargetModeWord = targetModeWord;

    if (FOTA_MapProvisionResult(result, FOTA_HANDLER_RESULT_ACTIVATION_FAILED) != E_OK)
    {
        return E_NOT_OK;
    }

    g_fotaHandlerContext.activationArmed = 1U;
    g_fotaHandlerContext.handlerState = FOTA_HANDLER_STATE_ACTIVATION_ARMED;
    return E_OK;
}

FotaHandlerStateType FOTA_GetHandlerState(void)
{
    return g_fotaHandlerContext.handlerState;
}

FotaHandlerResultType FOTA_GetLastHandlerResult(void)
{
    return g_fotaHandlerContext.lastHandlerResult;
}

FotaChunkStateType FOTA_GetChunkState(void)
{
    return g_fotaHandlerContext.chunkState;
}

uint32 FOTA_GetExpectedImageLength(void)
{
    return g_fotaHandlerContext.expectedImageLength;
}

uint32 FOTA_GetExpectedImageCrc(void)
{
    return g_fotaHandlerContext.expectedImageCrc;
}

uint32 FOTA_GetTotalReceivedBytes(void)
{
    return g_fotaHandlerContext.totalReceivedBytes;
}

uint32 FOTA_GetCurrentChunkLength(void)
{
    return g_fotaHandlerContext.length;
}

uint8 FOTA_GetCurrentBlockSequenceCounter(void)
{
    return g_fotaHandlerContext.blockSequenceCounter;
}

uint32 FOTA_GetLastUpdateResult(void)
{
    return (uint32)g_fotaHandlerContext.lastUpdateResult;
}

uint32 FOTA_GetLastProvisionResult(void)
{
    return (uint32)g_fotaHandlerContext.lastProvisionResult;
}

uint32 FOTA_GetLastSwapEntryIndex(void)
{
    return g_fotaHandlerContext.lastSwapEntryIndex;
}

uint32 FOTA_GetLastSwapTargetModeWord(void)
{
    return g_fotaHandlerContext.lastSwapTargetModeWord;
}

const void *FOTA_GetUpdateDebug(void)
{
    return (const void *)SotaUpdate_GetDebug();
}
