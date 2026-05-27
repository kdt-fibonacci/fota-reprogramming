#include <string.h>

#include <IfxDmu_reg.h>
#include <Sota/Update/Sota_UpdateCore.h>
#include <Sota/Sota_Tc37x_Config.h>
#include <Sota/Flash/Sota_FlashTc37x.h>
#include <Sota/Crc/crc32.h>

#define SOTA_UPDATE_PAGE_SIZE       (32u)
#define SOTA_UPDATE_INVALID_OFFSET  (0xFFFFFFFFu)

typedef struct
{
    SotaUpdateState_t state;
    uint32 inactiveBase;
    uint32 inactiveEnd;
    uint32 imageLength;
    uint32 expectedCrc;
    uint32 receivedBytes;
    uint32 programmedBytes;
    uint8 pageBuffer[SOTA_UPDATE_PAGE_SIZE];
    uint32 currentPageFill;
    IfxFlash_FlashType flashType;
    SotaUpdateDebug_t debug;
} SotaUpdateContext_t;

static SotaUpdateContext_t sotaUpdate;

static uint32 SotaUpdate_AlignUp32(uint32 value)
{
    return ((value + (SOTA_UPDATE_PAGE_SIZE - 1u)) / SOTA_UPDATE_PAGE_SIZE) * SOTA_UPDATE_PAGE_SIZE;
}

static void SotaUpdate_FillPageBuffer(void)
{
    uint32 index;

    for (index = 0u; index < SOTA_UPDATE_PAGE_SIZE; index++)
    {
        sotaUpdate.pageBuffer[index] = 0xFFu;
    }
}

static void SotaUpdate_SetState(SotaUpdateState_t state)
{
    sotaUpdate.state = state;
    sotaUpdate.debug.state = (uint32)state;
}

static void SotaUpdate_UpdateProgressDebug(void)
{
    sotaUpdate.debug.receivedBytes = sotaUpdate.receivedBytes;
    sotaUpdate.debug.programmedBytes = sotaUpdate.programmedBytes;
    sotaUpdate.debug.currentPageFill = sotaUpdate.currentPageFill;
    sotaUpdate.debug.state = (uint32)sotaUpdate.state;
}

static SotaUpdateResult_t SotaUpdate_SetError(SotaUpdateResult_t result)
{
    SotaUpdate_SetState(SOTA_UPDATE_STATE_ERROR);
    sotaUpdate.debug.finalizeResult = (uint32)result;
    SotaUpdate_UpdateProgressDebug();
    return result;
}

static boolean SotaUpdate_GetRangeEnd(uint32 start, uint32 len, uint32 *end)
{
    if ((len == 0u) || (end == NULL_PTR))
    {
        return FALSE;
    }

    if (start > (0xFFFFFFFFu - (len - 1u)))
    {
        return FALSE;
    }

    *end = start + len - 1u;
    return TRUE;
}

static SotaUpdateResult_t SotaUpdate_ProgramCurrentPage(void)
{
    uint8 flashResult;
    uint32 pageAddr;

    if (sotaUpdate.programmedBytes > (sotaUpdate.debug.paddedImageLength - SOTA_UPDATE_PAGE_SIZE))
    {
        return SotaUpdate_SetError(SOTA_UPDATE_PROGRAM_FAILED);
    }

    pageAddr = sotaUpdate.inactiveBase + sotaUpdate.programmedBytes;
    SotaUpdate_SetState(SOTA_UPDATE_STATE_PROGRAMMING);
    flashResult = SotaFlash_ProgramPage32(0u, pageAddr, &sotaUpdate.pageBuffer[0], sotaUpdate.flashType);
    sotaUpdate.debug.dmuErrAfterProgram = MODULE_DMU.HF_ERRSR.U;

    if (flashResult != FLASH_RESULT_OK)
    {
        sotaUpdate.debug.programFailOffset = sotaUpdate.programmedBytes;
        return SotaUpdate_SetError(SOTA_UPDATE_PROGRAM_FAILED);
    }

    sotaUpdate.programmedBytes += SOTA_UPDATE_PAGE_SIZE;
    sotaUpdate.currentPageFill = 0u;
    SotaUpdate_FillPageBuffer();
    SotaUpdate_SetState(SOTA_UPDATE_STATE_RECEIVING);
    SotaUpdate_UpdateProgressDebug();

    return SOTA_UPDATE_OK;
}

void SotaUpdate_Reset(void)
{
    memset(&sotaUpdate, 0, sizeof(sotaUpdate));
    SotaUpdate_FillPageBuffer();
    sotaUpdate.debug.programFailOffset = SOTA_UPDATE_INVALID_OFFSET;
    sotaUpdate.debug.verifyFailOffset = SOTA_UPDATE_INVALID_OFFSET;
    sotaUpdate.debug.eraseResult = FLASH_RESULT_OK;
    sotaUpdate.debug.finalizeResult = SOTA_UPDATE_OK;
    sotaUpdate.debug.done = 0u;
    SotaUpdate_SetState(SOTA_UPDATE_STATE_IDLE);
}

SotaUpdateResult_t SotaUpdate_Begin(uint32 imageLength, uint32 expectedCrc)
{
    uint32 endAddr;
    uint32 eraseAddr;
    uint32 sectorIndex;
    uint8 flashResult;

    SotaUpdate_Reset();
    sotaUpdate.imageLength = imageLength;
    sotaUpdate.expectedCrc = expectedCrc;
    sotaUpdate.debug.imageLength = imageLength;
    sotaUpdate.debug.expectedCrc = expectedCrc;

    if ((imageLength == 0u) ||
        (imageLength > TC37X_PFLASH_BANK_SIZE) ||
        (imageLength > (0xFFFFFFFFu - (SOTA_UPDATE_PAGE_SIZE - 1u))))
    {
        return SotaUpdate_SetError(SOTA_UPDATE_INVALID_LENGTH);
    }

    sotaUpdate.inactiveBase = SotaTc37x_GetInactiveBankStart();
    sotaUpdate.inactiveEnd = SotaTc37x_GetInactiveBankEnd();
    sotaUpdate.debug.inactiveBase = sotaUpdate.inactiveBase;
    sotaUpdate.debug.inactiveEnd = sotaUpdate.inactiveEnd;

    if ((sotaUpdate.inactiveBase == SOTA_INVALID_ADDRESS) ||
        (sotaUpdate.inactiveEnd == SOTA_INVALID_ADDRESS) ||
        (sotaUpdate.inactiveEnd < sotaUpdate.inactiveBase) ||
        (SotaTc37x_GetPFlashType(sotaUpdate.inactiveBase, &sotaUpdate.flashType) == FALSE))
    {
        return SotaUpdate_SetError(SOTA_UPDATE_INVALID_BANK);
    }

    sotaUpdate.debug.paddedImageLength = SotaUpdate_AlignUp32(imageLength);
    sotaUpdate.debug.sectorCount =
        (sotaUpdate.debug.paddedImageLength + (TC37X_PFLASH_SECTOR_SIZE - 1u)) / TC37X_PFLASH_SECTOR_SIZE;

    if ((sotaUpdate.debug.sectorCount == 0u) ||
        (sotaUpdate.debug.sectorCount > (0xFFFFFFFFu / TC37X_PFLASH_SECTOR_SIZE)))
    {
        return SotaUpdate_SetError(SOTA_UPDATE_INVALID_LENGTH);
    }

    sotaUpdate.debug.eraseSize = sotaUpdate.debug.sectorCount * TC37X_PFLASH_SECTOR_SIZE;
    sotaUpdate.debug.eraseStart = sotaUpdate.inactiveBase;

    if ((SotaUpdate_GetRangeEnd(sotaUpdate.inactiveBase, sotaUpdate.debug.eraseSize, &sotaUpdate.debug.eraseEnd) == FALSE) ||
        (SotaUpdate_GetRangeEnd(sotaUpdate.inactiveBase, sotaUpdate.debug.paddedImageLength, &endAddr) == FALSE) ||
        (sotaUpdate.debug.eraseEnd > sotaUpdate.inactiveEnd) ||
        (endAddr > sotaUpdate.inactiveEnd) ||
        (sotaUpdate.debug.eraseEnd >= TC37X_PFLASH_NC_INVALID_START) ||
        (endAddr >= TC37X_PFLASH_NC_INVALID_START) ||
        (SotaFlash_ValidatePflashWrite(sotaUpdate.inactiveBase, sotaUpdate.debug.paddedImageLength) != FLASH_RESULT_OK))
    {
        return SotaUpdate_SetError(SOTA_UPDATE_INVALID_BANK);
    }

    SotaFlash_CopyPflashRoutinesToPspr();
    SotaUpdate_SetState(SOTA_UPDATE_STATE_ERASING);
    sotaUpdate.debug.eraseResult = FLASH_RESULT_OK;

    for (sectorIndex = 0u; sectorIndex < sotaUpdate.debug.sectorCount; sectorIndex++)
    {
        eraseAddr = sotaUpdate.debug.eraseStart + (sectorIndex * TC37X_PFLASH_SECTOR_SIZE);
        flashResult = SotaFlash_EraseSector(0u, eraseAddr, sotaUpdate.flashType);
        if (flashResult != FLASH_RESULT_OK)
        {
            sotaUpdate.debug.eraseResult = flashResult;
            sotaUpdate.debug.dmuErrAfterErase = MODULE_DMU.HF_ERRSR.U;
            return SotaUpdate_SetError(SOTA_UPDATE_ERASE_FAILED);
        }
    }

    sotaUpdate.debug.dmuErrAfterErase = MODULE_DMU.HF_ERRSR.U;
    SotaUpdate_SetState(SOTA_UPDATE_STATE_RECEIVING);
    SotaUpdate_UpdateProgressDebug();

    return SOTA_UPDATE_OK;
}

SotaUpdateResult_t SotaUpdate_WriteChunk(const uint8 *data, uint32 len)
{
    uint32 copied = 0u;
    uint32 available;
    uint32 copyLen;
    SotaUpdateResult_t result;

    if ((sotaUpdate.state != SOTA_UPDATE_STATE_RECEIVING) &&
        (sotaUpdate.state != SOTA_UPDATE_STATE_PROGRAMMING))
    {
        return SotaUpdate_SetError(SOTA_UPDATE_RESULT_STATE_ERROR);
    }

    if (len == 0u)
    {
        return SOTA_UPDATE_OK;
    }

    if (data == NULL_PTR)
    {
        return SotaUpdate_SetError(SOTA_UPDATE_INVALID_PARAM);
    }

    if ((sotaUpdate.receivedBytes > sotaUpdate.imageLength) ||
        (len > (sotaUpdate.imageLength - sotaUpdate.receivedBytes)))
    {
        return SotaUpdate_SetError(SOTA_UPDATE_INVALID_LENGTH);
    }

    while (copied < len)
    {
        available = SOTA_UPDATE_PAGE_SIZE - sotaUpdate.currentPageFill;
        copyLen = len - copied;
        if (copyLen > available)
        {
            copyLen = available;
        }

        memcpy(&sotaUpdate.pageBuffer[sotaUpdate.currentPageFill], &data[copied], copyLen);
        sotaUpdate.currentPageFill += copyLen;
        copied += copyLen;
        sotaUpdate.receivedBytes += copyLen;

        if (sotaUpdate.currentPageFill == SOTA_UPDATE_PAGE_SIZE)
        {
            result = SotaUpdate_ProgramCurrentPage();
            if (result != SOTA_UPDATE_OK)
            {
                return result;
            }
        }
    }

    SotaUpdate_UpdateProgressDebug();
    return SOTA_UPDATE_OK;
}

SotaUpdateResult_t SotaUpdate_FinalizeAndVerify(void)
{
    SotaUpdateResult_t result;

    if ((sotaUpdate.state != SOTA_UPDATE_STATE_RECEIVING) &&
        (sotaUpdate.state != SOTA_UPDATE_STATE_PROGRAMMING))
    {
        return SotaUpdate_SetError(SOTA_UPDATE_RESULT_STATE_ERROR);
    }

    if (sotaUpdate.receivedBytes != sotaUpdate.imageLength)
    {
        return SotaUpdate_SetError(SOTA_UPDATE_INVALID_LENGTH);
    }

    if (sotaUpdate.currentPageFill > 0u)
    {
        while (sotaUpdate.currentPageFill < SOTA_UPDATE_PAGE_SIZE)
        {
            sotaUpdate.pageBuffer[sotaUpdate.currentPageFill] = 0xFFu;
            sotaUpdate.currentPageFill++;
        }

        result = SotaUpdate_ProgramCurrentPage();
        if (result != SOTA_UPDATE_OK)
        {
            return result;
        }
    }

    SotaUpdate_SetState(SOTA_UPDATE_STATE_VERIFYING);
    sotaUpdate.debug.actualCrc = crc32(0u, (const uint8 *)sotaUpdate.inactiveBase, sotaUpdate.imageLength);
    sotaUpdate.debug.currentPageFill = sotaUpdate.currentPageFill;
    SotaUpdate_UpdateProgressDebug();

    if (sotaUpdate.debug.actualCrc != sotaUpdate.expectedCrc)
    {
        return SotaUpdate_SetError(SOTA_UPDATE_CRC_FAILED);
    }

    SotaUpdate_SetState(SOTA_UPDATE_STATE_VERIFIED);
    sotaUpdate.debug.finalizeResult = SOTA_UPDATE_OK;
    sotaUpdate.debug.done = 1u;
    SotaUpdate_UpdateProgressDebug();

    return SOTA_UPDATE_OK;
}

SotaUpdateState_t SotaUpdate_GetState(void)
{
    return sotaUpdate.state;
}

const SotaUpdateDebug_t *SotaUpdate_GetDebug(void)
{
    return &sotaUpdate.debug;
}
