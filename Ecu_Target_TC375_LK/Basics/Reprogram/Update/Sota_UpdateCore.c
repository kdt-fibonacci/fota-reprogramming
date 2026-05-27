#include "Sota_UpdateCore.h"

#include <string.h>

#include "../Flash/Sota_FlashTc37x.h"

#define SOTA_UPDATECORE_CRC_INIT             (0xFFFFFFFFU)
#define SOTA_UPDATECORE_CRC_FINAL_XOR        (0xFFFFFFFFU)
#define SOTA_UPDATECORE_CRC_POLY             (0xEDB88320U)
#define SOTA_UPDATECORE_ERASED_BYTE          (0xFFU)
#define SOTA_UPDATECORE_INVALID_ADDRESS      (0xFFFFFFFFU)
#define SOTA_UPDATECORE_VERIFY_STEP_BYTE     (256U)
#define SOTA_UPDATECORE_PAGE_SIZE_BYTE       (SOTA_FLASH_PFLASH_PAGE_SIZE_BYTE)

#define SOTA_UPDATECORE_ALIGN_DOWN(value, align) \
    ((uint32)(value) / (uint32)(align) * (uint32)(align))

#define SOTA_UPDATECORE_ALIGN_UP(value, align) \
    ((((uint32)(value)) + ((uint32)(align) - 1U)) / (uint32)(align) * (uint32)(align))

#define SOTA_UPDATECORE_MIN(a, b) (((a) < (b)) ? (a) : (b))

typedef struct
{
    SotaUpdateCore_Progress_t progress;
    uint32 eraseOffset;
    uint32 verifyOffset;
    uint32 streamCrcRaw;
    uint32 verifyCrcRaw;
    uint32 pageAddress;
    uint32 pageFill;
    boolean pageDirty;
    uint8 pageBuffer[SOTA_UPDATECORE_PAGE_SIZE_BYTE];
} SotaUpdateCore_Context_t;

static SotaUpdateCore_Context_t g_sotaUpdateCore;
static uint8 g_sotaUpdateCoreProgrammedBitmap[12288];

volatile uint32 g_sotaUpdateCoreDebugLastChunkOffset;
volatile uint32 g_sotaUpdateCoreDebugLastChunkLength;
volatile uint32 g_sotaUpdateCoreDebugLastChunkExpectedCrc;
volatile uint32 g_sotaUpdateCoreDebugLastChunkCalculatedCrc;
volatile uint8 g_sotaUpdateCoreDebugLastChunkData0;
volatile uint8 g_sotaUpdateCoreDebugLastChunkData1;
volatile uint8 g_sotaUpdateCoreDebugLastChunkData2;
volatile uint8 g_sotaUpdateCoreDebugLastChunkData3;

static void SotaUpdateCore_ResetContext(void);
static SotaUpdateCore_Result_t SotaUpdateCore_SetError(SotaUpdateCore_Result_t result);
static SotaUpdateCore_Result_t SotaUpdateCore_MapFlashResult(SotaFlash_Result_t result);
static uint32 SotaUpdateCore_Crc32Init(void);
static uint32 SotaUpdateCore_Crc32Update(uint32 rawCrc,
                                         const uint8 *data,
                                         uint32 length);
static uint32 SotaUpdateCore_Crc32Finalize(uint32 rawCrc);
static uint32 SotaUpdateCore_CalcCrc32(const uint8 *data, uint32 length);
static boolean SotaUpdateCore_IsRangeInsideImage(uint32 offset,
                                                 uint32 length);
static SotaUpdateCore_Result_t SotaUpdateCore_ProgramCurrentPage(void);
static void SotaUpdateCore_StartPage(uint32 pageAddress);
static void SotaUpdateCore_ClearPage(void);

SotaUpdateCore_Result_t SotaUpdateCore_Prepare(uint32 imageSize,
                                               uint32 expectedImageCrc)
{
    SotaFlash_Bank_t inactiveBank;
    SotaFlash_Result_t flashResult;
    uint32 sectorSize;

    SotaUpdateCore_ResetContext();

    if (imageSize == 0U)
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INVALID_PARAM);
    }

    inactiveBank = SotaFlash_GetInactiveBank();
    if (inactiveBank == SOTA_FLASH_BANK_UNKNOWN)
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INVALID_RANGE);
    }

    g_sotaUpdateCore.progress.targetBank = (uint8)inactiveBank;
    g_sotaUpdateCore.progress.inactiveBankBase = SotaFlash_GetBankStart(inactiveBank);
    g_sotaUpdateCore.progress.inactiveBankSize = SotaFlash_GetBankSize(inactiveBank);
    g_sotaUpdateCore.progress.imageSize = imageSize;
    g_sotaUpdateCore.progress.expectedImageCrc = expectedImageCrc;

    if ((g_sotaUpdateCore.progress.inactiveBankBase == 0U) ||
        (g_sotaUpdateCore.progress.inactiveBankSize == 0U) ||
        (imageSize > g_sotaUpdateCore.progress.inactiveBankSize))
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INVALID_RANGE);
    }

    flashResult =
        SotaFlash_ValidateInactivePflashRange(g_sotaUpdateCore.progress.inactiveBankBase,
                                              imageSize);
    if (flashResult != SOTA_FLASH_RESULT_OK)
    {
        return SotaUpdateCore_SetError(SotaUpdateCore_MapFlashResult(flashResult));
    }

    sectorSize = SotaFlash_GetPflashSectorSize();
    if (sectorSize == 0U)
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INTERNAL_ERROR);
    }

    g_sotaUpdateCore.progress.eraseBytes =
        SOTA_UPDATECORE_ALIGN_UP(imageSize, sectorSize);
    if (g_sotaUpdateCore.progress.eraseBytes >
        g_sotaUpdateCore.progress.inactiveBankSize)
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INVALID_RANGE);
    }

    flashResult = SotaFlash_CopyPflashRoutinesToPspr();
    if (flashResult != SOTA_FLASH_RESULT_OK)
    {
        return SotaUpdateCore_SetError(SotaUpdateCore_MapFlashResult(flashResult));
    }

    (void)memset(g_sotaUpdateCoreProgrammedBitmap, 0, sizeof(g_sotaUpdateCoreProgrammedBitmap));
    g_sotaUpdateCore.progress.state = SOTA_UPDATECORE_STATE_PREPARED;
    return SOTA_UPDATECORE_RESULT_OK;
}

SotaUpdateCore_Result_t SotaUpdateCore_EraseStep(void)
{
    SotaFlash_Result_t flashResult;
    uint32 sectorSize;
    uint32 sectorAddress;

    /*
     * This performs one real PFLASH erase at most. It must be driven from the
     * background UpdateManager step, never directly from CAN-FD RX context.
     */
    if ((g_sotaUpdateCore.progress.state != SOTA_UPDATECORE_STATE_PREPARED) &&
        (g_sotaUpdateCore.progress.state != SOTA_UPDATECORE_STATE_ERASING))
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INVALID_STATE);
    }

    if (g_sotaUpdateCore.eraseOffset >= g_sotaUpdateCore.progress.eraseBytes)
    {
        g_sotaUpdateCore.progress.state = SOTA_UPDATECORE_STATE_ERASED;
        return SOTA_UPDATECORE_RESULT_OK;
    }

    sectorSize = SotaFlash_GetPflashSectorSize();
    if (sectorSize == 0U)
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INTERNAL_ERROR);
    }

    g_sotaUpdateCore.progress.state = SOTA_UPDATECORE_STATE_ERASING;
    sectorAddress = g_sotaUpdateCore.progress.inactiveBankBase +
                    g_sotaUpdateCore.eraseOffset;

    flashResult = SotaFlash_EraseSector(sectorAddress);
    if (flashResult != SOTA_FLASH_RESULT_OK)
    {
        return SotaUpdateCore_SetError(SotaUpdateCore_MapFlashResult(flashResult));
    }

    g_sotaUpdateCore.eraseOffset += sectorSize;
    g_sotaUpdateCore.progress.erasedBytes =
        SOTA_UPDATECORE_MIN(g_sotaUpdateCore.eraseOffset,
                            g_sotaUpdateCore.progress.imageSize);

    if (g_sotaUpdateCore.eraseOffset < g_sotaUpdateCore.progress.eraseBytes)
    {
        return SOTA_UPDATECORE_RESULT_PENDING;
    }

    g_sotaUpdateCore.progress.state = SOTA_UPDATECORE_STATE_ERASED;
    return SOTA_UPDATECORE_RESULT_OK;
}

SotaUpdateCore_Result_t SotaUpdateCore_WriteChunk(uint32 offset,
                                                  const uint8 *data,
                                                  uint32 length,
                                                  uint32 chunkCrc)
{
    SotaUpdateCore_Result_t result = SOTA_UPDATECORE_RESULT_OK;
    SotaFlash_Result_t flashResult;
    uint32 absoluteAddress;
    uint32 currentPageAddress;
    uint32 pageOffset;
    uint32 copyLength;
    uint32 sourceOffset = 0U;
    uint32 remaining = length;
    uint32 calculatedCrc;

    /*
     * DATA parsing may happen from CAN-FD RX, but this function programs real
     * PFLASH pages and must only be called by the background state machine.
     */
    if ((data == NULL_PTR) || (length == 0U))
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INVALID_PARAM);
    }

    if ((g_sotaUpdateCore.progress.state != SOTA_UPDATECORE_STATE_ERASED) &&
        (g_sotaUpdateCore.progress.state != SOTA_UPDATECORE_STATE_WRITING))
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INVALID_STATE);
    }

    /* Skip Padding Detection and CRC Sync */
    if (offset > g_sotaUpdateCore.progress.receivedBytes)
    {
        uint32 skipLen = offset - g_sotaUpdateCore.progress.receivedBytes;
        uint8 ffBuffer[64];
        uint32 tempLen = skipLen;
        (void)memset(ffBuffer, 0xFF, sizeof(ffBuffer));
        while (tempLen > 0U)
        {
            uint32 chunkLen = SOTA_UPDATECORE_MIN(tempLen, sizeof(ffBuffer));
            g_sotaUpdateCore.streamCrcRaw = SotaUpdateCore_Crc32Update(g_sotaUpdateCore.streamCrcRaw, ffBuffer, chunkLen);
            tempLen -= chunkLen;
        }
        
        if (g_sotaUpdateCore.pageDirty == TRUE)
        {
            uint32 targetPageAddress = SOTA_UPDATECORE_ALIGN_DOWN(g_sotaUpdateCore.progress.inactiveBankBase + offset,
                                                                 SOTA_UPDATECORE_PAGE_SIZE_BYTE);
            if (g_sotaUpdateCore.pageAddress != targetPageAddress)
            {
                /* Flush the active dirty page since we are jumping to a different page address */
                g_sotaUpdateCore.pageFill = SOTA_UPDATECORE_PAGE_SIZE_BYTE;
                result = SotaUpdateCore_ProgramCurrentPage();
                if (result != SOTA_UPDATECORE_RESULT_OK)
                {
                    return SotaUpdateCore_SetError(result);
                }
            }
            else
            {
                /* Same page, adjust pageFill to skip the padding bytes */
                g_sotaUpdateCore.pageFill = (g_sotaUpdateCore.progress.inactiveBankBase + offset) - g_sotaUpdateCore.pageAddress;
            }
        }
        
        g_sotaUpdateCore.progress.receivedBytes = offset;
        g_sotaUpdateCore.progress.programmedBytes = offset;
    }

    if (offset != g_sotaUpdateCore.progress.receivedBytes)
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INVALID_RANGE);
    }

    if (SotaUpdateCore_IsRangeInsideImage(offset, length) == FALSE)
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INVALID_RANGE);
    }

    calculatedCrc = SotaUpdateCore_CalcCrc32(data, length);
    g_sotaUpdateCoreDebugLastChunkOffset = offset;
    g_sotaUpdateCoreDebugLastChunkLength = length;
    g_sotaUpdateCoreDebugLastChunkExpectedCrc = chunkCrc;
    g_sotaUpdateCoreDebugLastChunkCalculatedCrc = calculatedCrc;
    g_sotaUpdateCoreDebugLastChunkData0 = data[0];
    g_sotaUpdateCoreDebugLastChunkData1 = (length > 1U) ? data[1] : 0U;
    g_sotaUpdateCoreDebugLastChunkData2 = (length > 2U) ? data[2] : 0U;
    g_sotaUpdateCoreDebugLastChunkData3 = (length > 3U) ? data[3] : 0U;
    if (calculatedCrc != chunkCrc)
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_CRC_ERROR);
    }

    flashResult =
        SotaFlash_ValidateInactivePflashRange(g_sotaUpdateCore.progress.inactiveBankBase + offset,
                                              length);
    if (flashResult != SOTA_FLASH_RESULT_OK)
    {
        return SotaUpdateCore_SetError(SotaUpdateCore_MapFlashResult(flashResult));
    }

    g_sotaUpdateCore.progress.state = SOTA_UPDATECORE_STATE_WRITING;

    while ((remaining > 0U) && (result == SOTA_UPDATECORE_RESULT_OK))
    {
        absoluteAddress = g_sotaUpdateCore.progress.inactiveBankBase +
                          offset +
                          sourceOffset;
        currentPageAddress =
            SOTA_UPDATECORE_ALIGN_DOWN(absoluteAddress,
                                       SOTA_UPDATECORE_PAGE_SIZE_BYTE);
        pageOffset = absoluteAddress - currentPageAddress;

        if (g_sotaUpdateCore.pageDirty == FALSE)
        {
            SotaUpdateCore_StartPage(currentPageAddress);
            if (currentPageAddress == SOTA_UPDATECORE_ALIGN_DOWN(g_sotaUpdateCore.progress.inactiveBankBase + offset, SOTA_UPDATECORE_PAGE_SIZE_BYTE))
            {
                g_sotaUpdateCore.pageFill = pageOffset;
            }
        }
        else if (g_sotaUpdateCore.pageAddress != currentPageAddress)
        {
            if (g_sotaUpdateCore.pageFill == SOTA_UPDATECORE_PAGE_SIZE_BYTE)
            {
                result = SotaUpdateCore_ProgramCurrentPage();
                if (result == SOTA_UPDATECORE_RESULT_OK)
                {
                    SotaUpdateCore_StartPage(currentPageAddress);
                }
            }
            else
            {
                result = SOTA_UPDATECORE_RESULT_INVALID_RANGE;
            }
        }
        else
        {
            /* Continue filling the current partial page. */
        }

        if (result != SOTA_UPDATECORE_RESULT_OK)
        {
            break;
        }

        if (pageOffset != g_sotaUpdateCore.pageFill)
        {
            result = SOTA_UPDATECORE_RESULT_INVALID_RANGE;
            break;
        }

        copyLength =
            SOTA_UPDATECORE_MIN(remaining,
                                SOTA_UPDATECORE_PAGE_SIZE_BYTE - pageOffset);
        (void)memcpy(&g_sotaUpdateCore.pageBuffer[pageOffset],
                     &data[sourceOffset],
                     copyLength);
        g_sotaUpdateCore.pageFill += copyLength;
        g_sotaUpdateCore.pageDirty = TRUE;

        if (g_sotaUpdateCore.pageFill == SOTA_UPDATECORE_PAGE_SIZE_BYTE)
        {
            result = SotaUpdateCore_ProgramCurrentPage();
        }

        remaining -= copyLength;
        sourceOffset += copyLength;
    }

    if (result != SOTA_UPDATECORE_RESULT_OK)
    {
        return SotaUpdateCore_SetError(result);
    }

    g_sotaUpdateCore.progress.receivedBytes = offset + length;
    g_sotaUpdateCore.streamCrcRaw =
        SotaUpdateCore_Crc32Update(g_sotaUpdateCore.streamCrcRaw,
                                   data,
                                   length);

    return SOTA_UPDATECORE_RESULT_OK;
}

SotaUpdateCore_Result_t SotaUpdateCore_VerifyStep(void)
{
    SotaFlash_Result_t flashResult;
    uint8 verifyBuffer[SOTA_UPDATECORE_VERIFY_STEP_BYTE];
    uint32 remaining;
    uint32 stepLength;

    if (g_sotaUpdateCore.progress.state == SOTA_UPDATECORE_STATE_VERIFIED)
    {
        return SOTA_UPDATECORE_RESULT_OK;
    }

    if ((g_sotaUpdateCore.progress.state != SOTA_UPDATECORE_STATE_FINALIZED) &&
        (g_sotaUpdateCore.progress.state != SOTA_UPDATECORE_STATE_VERIFYING))
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INVALID_STATE);
    }

    if (g_sotaUpdateCore.progress.state == SOTA_UPDATECORE_STATE_FINALIZED)
    {
        g_sotaUpdateCore.verifyOffset = 0U;
        g_sotaUpdateCore.verifyCrcRaw = SotaUpdateCore_Crc32Init();
        g_sotaUpdateCore.progress.verifiedBytes = 0U;
        g_sotaUpdateCore.progress.state = SOTA_UPDATECORE_STATE_VERIFYING;
    }

    remaining = g_sotaUpdateCore.progress.imageSize - g_sotaUpdateCore.verifyOffset;
    stepLength = SOTA_UPDATECORE_MIN(remaining, SOTA_UPDATECORE_VERIFY_STEP_BYTE);

    if (stepLength > 0U)
    {
        uint32 p;
        uint32 pagesInStep = stepLength / 32U;
        
        for (p = 0U; p < pagesInStep; p++)
        {
            uint32 pageOffset = g_sotaUpdateCore.verifyOffset + (p * 32U);
            uint32 pageIndex = pageOffset / 32U;
            uint32 pageAddr = g_sotaUpdateCore.progress.inactiveBankBase + pageOffset;
            
            if ((g_sotaUpdateCoreProgrammedBitmap[pageIndex / 8U] & (uint8)(1U << (pageIndex % 8U))) != 0U)
            {
                flashResult = SotaFlash_Read(pageAddr, &verifyBuffer[p * 32U], 32U);
                if (flashResult != SOTA_FLASH_RESULT_OK)
                {
                    return SotaUpdateCore_SetError(SotaUpdateCore_MapFlashResult(flashResult));
                }
            }
            else
            {
                (void)memset(&verifyBuffer[p * 32U], 0xFF, 32U);
            }
        }

        g_sotaUpdateCore.verifyCrcRaw =
            SotaUpdateCore_Crc32Update(g_sotaUpdateCore.verifyCrcRaw,
                                       verifyBuffer,
                                       stepLength);
        g_sotaUpdateCore.verifyOffset += stepLength;
        g_sotaUpdateCore.progress.verifiedBytes = g_sotaUpdateCore.verifyOffset;
    }

    if (g_sotaUpdateCore.verifyOffset < g_sotaUpdateCore.progress.imageSize)
    {
        return SOTA_UPDATECORE_RESULT_PENDING;
    }

    if (SotaUpdateCore_Crc32Finalize(g_sotaUpdateCore.verifyCrcRaw) !=
        g_sotaUpdateCore.progress.expectedImageCrc)
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_VERIFY_ERROR);
    }

    g_sotaUpdateCore.progress.state = SOTA_UPDATECORE_STATE_VERIFIED;
    return SOTA_UPDATECORE_RESULT_OK;
}

SotaUpdateCore_Result_t SotaUpdateCore_Finalize(void)
{
    SotaUpdateCore_Result_t result;
    uint32 imageCrc;

    if ((g_sotaUpdateCore.progress.state != SOTA_UPDATECORE_STATE_WRITING) &&
        (g_sotaUpdateCore.progress.state != SOTA_UPDATECORE_STATE_ERASED))
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INVALID_STATE);
    }

    if (g_sotaUpdateCore.progress.receivedBytes !=
        g_sotaUpdateCore.progress.imageSize)
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_INVALID_STATE);
    }

    if (g_sotaUpdateCore.pageDirty == TRUE)
    {
        result = SotaUpdateCore_ProgramCurrentPage();
        if (result != SOTA_UPDATECORE_RESULT_OK)
        {
            return SotaUpdateCore_SetError(result);
        }
    }

    /* Fill skipped unprogrammed pages with 0xFF locally to write valid ECC and prevent CPU startup bus traps */
    {
        uint32 totalPages = SOTA_UPDATECORE_ALIGN_UP(g_sotaUpdateCore.progress.imageSize, SOTA_UPDATECORE_PAGE_SIZE_BYTE) / SOTA_UPDATECORE_PAGE_SIZE_BYTE;
        uint32 p;
        uint8 ffBuffer[SOTA_UPDATECORE_PAGE_SIZE_BYTE];
        (void)memset(ffBuffer, 0xFF, sizeof(ffBuffer));

        for (p = 0U; p < totalPages; p++)
        {
            if ((g_sotaUpdateCoreProgrammedBitmap[p / 8U] & (uint8)(1U << (p % 8U))) == 0U)
            {
                uint32 pageAddr = g_sotaUpdateCore.progress.inactiveBankBase + (p * SOTA_UPDATECORE_PAGE_SIZE_BYTE);
                SotaFlash_Result_t flashResult = SotaFlash_ProgramPage32(pageAddr, ffBuffer);
                if (flashResult != SOTA_FLASH_RESULT_OK)
                {
                    return SotaUpdateCore_SetError(SotaUpdateCore_MapFlashResult(flashResult));
                }
                g_sotaUpdateCoreProgrammedBitmap[p / 8U] |= (uint8)(1U << (p % 8U));
            }
        }
    }

    imageCrc = SotaUpdateCore_Crc32Finalize(g_sotaUpdateCore.streamCrcRaw);
    if (imageCrc != g_sotaUpdateCore.progress.expectedImageCrc)
    {
        return SotaUpdateCore_SetError(SOTA_UPDATECORE_RESULT_CRC_ERROR);
    }

    g_sotaUpdateCore.progress.programmedBytes = g_sotaUpdateCore.progress.imageSize;
    g_sotaUpdateCore.progress.state = SOTA_UPDATECORE_STATE_FINALIZED;
    return SOTA_UPDATECORE_RESULT_OK;
}

SotaUpdateCore_Result_t SotaUpdateCore_Abort(void)
{
    SotaUpdateCore_ResetContext();
    return SOTA_UPDATECORE_RESULT_OK;
}

void SotaUpdateCore_GetProgress(SotaUpdateCore_Progress_t *outProgress)
{
    if (outProgress != NULL_PTR)
    {
        *outProgress = g_sotaUpdateCore.progress;
    }
}

static void SotaUpdateCore_ResetContext(void)
{
    (void)memset(&g_sotaUpdateCore, 0, sizeof(g_sotaUpdateCore));
    g_sotaUpdateCore.progress.state = SOTA_UPDATECORE_STATE_IDLE;
    g_sotaUpdateCore.progress.targetBank = (uint8)SOTA_FLASH_BANK_UNKNOWN;
    g_sotaUpdateCore.streamCrcRaw = SotaUpdateCore_Crc32Init();
    g_sotaUpdateCore.verifyCrcRaw = SotaUpdateCore_Crc32Init();
    SotaUpdateCore_ClearPage();
}

static SotaUpdateCore_Result_t SotaUpdateCore_SetError(SotaUpdateCore_Result_t result)
{
    if ((result != SOTA_UPDATECORE_RESULT_OK) &&
        (result != SOTA_UPDATECORE_RESULT_PENDING))
    {
        g_sotaUpdateCore.progress.state = SOTA_UPDATECORE_STATE_ERROR;
        g_sotaUpdateCore.progress.lastError = (uint32)result;
    }

    return result;
}

static SotaUpdateCore_Result_t SotaUpdateCore_MapFlashResult(SotaFlash_Result_t result)
{
    SotaUpdateCore_Result_t coreResult;

    switch (result)
    {
    case SOTA_FLASH_RESULT_OK:
        coreResult = SOTA_UPDATECORE_RESULT_OK;
        break;

    case SOTA_FLASH_RESULT_INVALID_PARAM:
        coreResult = SOTA_UPDATECORE_RESULT_INVALID_PARAM;
        break;

    case SOTA_FLASH_RESULT_INVALID_LENGTH:
        coreResult = SOTA_UPDATECORE_RESULT_INVALID_PARAM;
        break;

    case SOTA_FLASH_RESULT_INVALID_ADDRESS:
    case SOTA_FLASH_RESULT_INVALID_RANGE:
    case SOTA_FLASH_RESULT_ACTIVE_BANK_ACCESS:
        coreResult = SOTA_UPDATECORE_RESULT_INVALID_RANGE;
        break;

    case SOTA_FLASH_RESULT_VERIFY_ERROR:
        coreResult = SOTA_UPDATECORE_RESULT_VERIFY_ERROR;
        break;

    case SOTA_FLASH_RESULT_FLASH_BUSY_TIMEOUT:
    case SOTA_FLASH_RESULT_HARDWARE_ERROR:
    case SOTA_FLASH_RESULT_UNSUPPORTED:
    default:
        coreResult = SOTA_UPDATECORE_RESULT_FLASH_ERROR;
        break;
    }

    return coreResult;
}

static uint32 SotaUpdateCore_Crc32Init(void)
{
    return SOTA_UPDATECORE_CRC_INIT;
}

static uint32 SotaUpdateCore_Crc32Update(uint32 rawCrc,
                                         const uint8 *data,
                                         uint32 length)
{
    uint32 crc = rawCrc;
    uint32 i;
    uint32 bit;

    if (data == NULL_PTR)
    {
        return rawCrc;
    }

    for (i = 0U; i < length; i++)
    {
        crc ^= (uint32)data[i];

        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1U) ^ SOTA_UPDATECORE_CRC_POLY;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

static uint32 SotaUpdateCore_Crc32Finalize(uint32 rawCrc)
{
    return rawCrc ^ SOTA_UPDATECORE_CRC_FINAL_XOR;
}

static uint32 SotaUpdateCore_CalcCrc32(const uint8 *data, uint32 length)
{
    return SotaUpdateCore_Crc32Finalize(
        SotaUpdateCore_Crc32Update(SotaUpdateCore_Crc32Init(),
                                   data,
                                   length));
}

static boolean SotaUpdateCore_IsRangeInsideImage(uint32 offset,
                                                 uint32 length)
{
    uint32 endOffset;

    if (length == 0U)
    {
        return FALSE;
    }

    endOffset = offset + length;
    if (endOffset < offset)
    {
        return FALSE;
    }

    return (endOffset <= g_sotaUpdateCore.progress.imageSize) ? TRUE : FALSE;
}

static SotaUpdateCore_Result_t SotaUpdateCore_ProgramCurrentPage(void)
{
    SotaFlash_Result_t flashResult;
    uint32 pageEndOffset;

    if (g_sotaUpdateCore.pageDirty == FALSE)
    {
        return SOTA_UPDATECORE_RESULT_OK;
    }

    if ((g_sotaUpdateCore.pageAddress == SOTA_UPDATECORE_INVALID_ADDRESS) ||
        (g_sotaUpdateCore.pageFill == 0U))
    {
        return SOTA_UPDATECORE_RESULT_INTERNAL_ERROR;
    }

    flashResult =
        SotaFlash_ValidateInactivePflashRange(g_sotaUpdateCore.pageAddress,
                                              SOTA_UPDATECORE_PAGE_SIZE_BYTE);
    if (flashResult != SOTA_FLASH_RESULT_OK)
    {
        return SotaUpdateCore_MapFlashResult(flashResult);
    }

    flashResult = SotaFlash_ProgramPage32(g_sotaUpdateCore.pageAddress,
                                          g_sotaUpdateCore.pageBuffer);
    if (flashResult != SOTA_FLASH_RESULT_OK)
    {
        return SotaUpdateCore_MapFlashResult(flashResult);
    }

    {
        uint32 pageIndex = (g_sotaUpdateCore.pageAddress - g_sotaUpdateCore.progress.inactiveBankBase) / SOTA_UPDATECORE_PAGE_SIZE_BYTE;
        g_sotaUpdateCoreProgrammedBitmap[pageIndex / 8U] |= (uint8)(1U << (pageIndex % 8U));
    }

    pageEndOffset =
        (g_sotaUpdateCore.pageAddress - g_sotaUpdateCore.progress.inactiveBankBase) +
        SOTA_UPDATECORE_PAGE_SIZE_BYTE;
    g_sotaUpdateCore.progress.programmedBytes =
        SOTA_UPDATECORE_MIN(pageEndOffset,
                            g_sotaUpdateCore.progress.imageSize);

    SotaUpdateCore_ClearPage();
    return SOTA_UPDATECORE_RESULT_OK;
}

static void SotaUpdateCore_StartPage(uint32 pageAddress)
{
    (void)memset(g_sotaUpdateCore.pageBuffer,
                 SOTA_UPDATECORE_ERASED_BYTE,
                 sizeof(g_sotaUpdateCore.pageBuffer));
    g_sotaUpdateCore.pageAddress = pageAddress;
    g_sotaUpdateCore.pageFill = 0U;
    g_sotaUpdateCore.pageDirty = TRUE;
}

static void SotaUpdateCore_ClearPage(void)
{
    (void)memset(g_sotaUpdateCore.pageBuffer,
                 SOTA_UPDATECORE_ERASED_BYTE,
                 sizeof(g_sotaUpdateCore.pageBuffer));
    g_sotaUpdateCore.pageAddress = SOTA_UPDATECORE_INVALID_ADDRESS;
    g_sotaUpdateCore.pageFill = 0U;
    g_sotaUpdateCore.pageDirty = FALSE;
}
