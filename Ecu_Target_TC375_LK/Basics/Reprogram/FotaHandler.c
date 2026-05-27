/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include <string.h>
#include "FotaHandler.h"

#ifndef NULL_PTR
#define NULL_PTR ((void *)0)
#endif

/*********************************************************************************************************************/
/*------------------------------------------------------Types--------------------------------------------------------*/
/*********************************************************************************************************************/

typedef struct
{
    FotaChunkStateType chunkState;
    uint8 buffer[FOTA_MAX_TRANSFER_DATA_LENGTH];
    uint32 length;
    uint8 blockSequenceCounter;
    uint32 totalReceivedBytes;
    SotaUpdateResult_t lastSotaResult;
} FotaHandlerContextType;

/*********************************************************************************************************************/
/*-------------------------------------------------Static Variables--------------------------------------------------*/
/*********************************************************************************************************************/

static FotaHandlerContextType g_fotaHandlerContext =
{
    FOTA_CHUNK_IDLE,
    {0U},
    0U,
    0U,
    0U,
    SOTA_UPDATE_OK
};

/*********************************************************************************************************************/
/*-----------------------------------------------Private Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static void FOTA_SetError(SotaUpdateResult_t result)
{
    g_fotaHandlerContext.lastSotaResult = result;
    g_fotaHandlerContext.chunkState = FOTA_CHUNK_ERROR;
}

static void FOTA_ClearChunkOnly(void)
{
    g_fotaHandlerContext.length = 0U;
    g_fotaHandlerContext.blockSequenceCounter = 0U;
}

/*********************************************************************************************************************/
/*------------------------------------------------Public Functions---------------------------------------------------*/
/*********************************************************************************************************************/

void FOTA_ResetContext(void)
{
    g_fotaHandlerContext.chunkState = FOTA_CHUNK_IDLE;
    FOTA_ClearChunkOnly();
    g_fotaHandlerContext.totalReceivedBytes = 0U;
    g_fotaHandlerContext.lastSotaResult = SOTA_UPDATE_OK;
}

Dcm_ReturnWriteMemoryType FOTA_ProcessTransferDataWrite(
    Dcm_OpStatusType OpStatus,
    const uint8 *DataPtr,
    uint32 DataLength,
    uint8 BlockSequenceCounter
)
{
    if (OpStatus == DCM_OP_INITIAL)
    {
        if ((DataPtr == NULL_PTR) ||
            (DataLength == 0U) ||
            (DataLength > FOTA_MAX_TRANSFER_DATA_LENGTH))
        {
            FOTA_SetError(SOTA_UPDATE_INVALID_PARAM);
            return DCM_WRITE_FAILED;
        }

        if (g_fotaHandlerContext.chunkState == FOTA_CHUNK_IDLE)
        {
            (void)memcpy(&g_fotaHandlerContext.buffer[0], DataPtr, DataLength);
            g_fotaHandlerContext.length = DataLength;
            g_fotaHandlerContext.blockSequenceCounter = BlockSequenceCounter;
            g_fotaHandlerContext.lastSotaResult = SOTA_UPDATE_OK;
            g_fotaHandlerContext.chunkState = FOTA_CHUNK_RECEIVED;

            return DCM_WRITE_PENDING;
        }

        if ((g_fotaHandlerContext.chunkState == FOTA_CHUNK_RECEIVED) ||
            (g_fotaHandlerContext.chunkState == FOTA_CHUNK_PROCESSING))
        {
            /* Existing chunk is still owned by FOTAHandlerMain(). Do not overwrite it. */
            return DCM_WRITE_PENDING;
        }

        if (g_fotaHandlerContext.chunkState == FOTA_CHUNK_DONE)
        {
            /*
             * The previous chunk was processed but DCM has not consumed its
             * DCM_OP_PENDING -> DCM_WRITE_OK transition yet. Accepting a new
             * INITIAL call here would drop the previous positive response.
             */
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
        return DCM_WRITE_FAILED;
    }

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
    g_fotaHandlerContext.lastSotaResult = result;

    if (result == SOTA_UPDATE_OK)
    {
        g_fotaHandlerContext.totalReceivedBytes += g_fotaHandlerContext.length;
        g_fotaHandlerContext.chunkState = FOTA_CHUNK_DONE;
    }
    else
    {
        g_fotaHandlerContext.chunkState = FOTA_CHUNK_ERROR;
    }
}

FotaChunkStateType FOTA_GetChunkState(void)
{
    return g_fotaHandlerContext.chunkState;
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

SotaUpdateResult_t FOTA_GetLastSotaResult(void)
{
    return g_fotaHandlerContext.lastSotaResult;
}
