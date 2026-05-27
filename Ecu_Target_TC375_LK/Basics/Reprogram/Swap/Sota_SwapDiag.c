#include "Sota_SwapDiag.h"

#include <string.h>

#include "../Flash/Sota_FlashTc37x.h"

#define SOTA_SWAPDIAG_UCB_SWAP_ORIG_INDEX      (23U)
#define SOTA_SWAPDIAG_UCB_SWAP_COPY_INDEX      (31U)
#define SOTA_SWAPDIAG_ENTRY_COUNT              (16U)
#define SOTA_SWAPDIAG_ENTRY_SIZE_BYTE          (16U)
#define SOTA_SWAPDIAG_ENTRY_MARKER_OFFSET      (8U)
#define SOTA_SWAPDIAG_UCB_CONFIRM_OFFSET       (496U)
#define SOTA_SWAPDIAG_UCB_CONFIRM_WORD         (0x43211234U)
#define SOTA_SWAPDIAG_ENTRY_CONFIRM_WORD       (0x57B5327FU)
#define SOTA_SWAPDIAG_MODE_PF0_ACTIVE          (0x55U)
#define SOTA_SWAPDIAG_MODE_PF1_ACTIVE          (0xAAU)
#define SOTA_SWAPDIAG_INVALID_ENTRY_INDEX      (0xFFFFU)

typedef uint32 SotaSwapDiag_EntryWords_t[4];

static SotaSwapManager_Result_t SotaSwapDiag_MapFlashResult(SotaFlash_Result_t result);
static uint32 SotaSwapDiag_GetEntryOffset(uint8 entryIndex);
static uint32 SotaSwapDiag_GetEntryMarkerOffset(uint8 entryIndex);
static uint32 SotaSwapDiag_GetTargetMode(uint8 targetBank);
static boolean SotaSwapDiag_IsValidMode(uint32 mode);
static boolean SotaSwapDiag_AreWordsAll(const uint32 *words, uint32 value);
static boolean SotaSwapDiag_IsEntryErased(const SotaSwapDiag_EntryWords_t words);
static boolean SotaSwapDiag_IsEntryForMode(uint8 ucbIndex,
                                           uint8 entryIndex,
                                           uint32 mode,
                                           const SotaSwapDiag_EntryWords_t words);
static SotaSwapManager_Result_t SotaSwapDiag_ReadEntry(uint8 ucbIndex,
                                                       uint8 entryIndex,
                                                       SotaSwapDiag_EntryWords_t words);
static SotaSwapManager_Result_t SotaSwapDiag_VerifyConfirmWord(uint8 ucbIndex);
static SotaSwapManager_Result_t SotaSwapDiag_FindFreeEntry(uint8 ucbIndex,
                                                           uint8 *outEntryIndex);
static SotaSwapManager_Result_t SotaSwapDiag_ProgramPageWords(uint8 ucbIndex,
                                                              uint32 offset,
                                                              const uint32 *words);
static SotaSwapManager_Result_t SotaSwapDiag_ProgramEntry(uint8 ucbIndex,
                                                          uint8 entryIndex,
                                                          uint32 mode);

volatile uint32 g_sotaSwapDiagDebugLastResult;
volatile uint32 g_sotaSwapDiagDebugLastTargetBank;
volatile uint32 g_sotaSwapDiagDebugLastOrigEntryIndex;
volatile uint32 g_sotaSwapDiagDebugLastCopyEntryIndex;

SotaSwapManager_Result_t SotaSwapDiag_AppendNextSwapEntry(uint8 targetBank,
                                                          uint16 *outEntryIndex)
{
    SotaTc37x_PflashBank_t activeBank;
    uint32 mode;
    uint8 origEntryIndex;
    uint8 copyEntryIndex;
    SotaSwapManager_Result_t result;

    if (outEntryIndex == NULL_PTR)
    {
        return SOTA_SWAP_MANAGER_RESULT_INVALID_PARAM;
    }

    *outEntryIndex = SOTA_SWAPDIAG_INVALID_ENTRY_INDEX;
    g_sotaSwapDiagDebugLastTargetBank = targetBank;
    g_sotaSwapDiagDebugLastOrigEntryIndex = SOTA_SWAPDIAG_INVALID_ENTRY_INDEX;
    g_sotaSwapDiagDebugLastCopyEntryIndex = SOTA_SWAPDIAG_INVALID_ENTRY_INDEX;

    if ((targetBank != (uint8)SOTA_TC37X_PFLASH_BANK_PF0) &&
        (targetBank != (uint8)SOTA_TC37X_PFLASH_BANK_PF1))
    {
        g_sotaSwapDiagDebugLastResult = SOTA_SWAP_MANAGER_RESULT_INVALID_PARAM;
        return SOTA_SWAP_MANAGER_RESULT_INVALID_PARAM;
    }

    activeBank = SotaTc37x_GetCurrentActiveBank();
    if ((activeBank == SOTA_TC37X_PFLASH_BANK_UNKNOWN) ||
        (activeBank == (SotaTc37x_PflashBank_t)targetBank))
    {
        g_sotaSwapDiagDebugLastResult = SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
        return SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
    }

    mode = SotaSwapDiag_GetTargetMode(targetBank);
    if (SotaSwapDiag_IsValidMode(mode) == FALSE)
    {
        g_sotaSwapDiagDebugLastResult = SOTA_SWAP_MANAGER_RESULT_INVALID_PARAM;
        return SOTA_SWAP_MANAGER_RESULT_INVALID_PARAM;
    }

    result = SotaSwapDiag_VerifyConfirmWord(SOTA_SWAPDIAG_UCB_SWAP_ORIG_INDEX);
    if (result != SOTA_SWAP_MANAGER_RESULT_OK)
    {
        g_sotaSwapDiagDebugLastResult = result;
        return result;
    }

    result = SotaSwapDiag_VerifyConfirmWord(SOTA_SWAPDIAG_UCB_SWAP_COPY_INDEX);
    if (result != SOTA_SWAP_MANAGER_RESULT_OK)
    {
        g_sotaSwapDiagDebugLastResult = result;
        return result;
    }

    result = SotaSwapDiag_FindFreeEntry(SOTA_SWAPDIAG_UCB_SWAP_ORIG_INDEX,
                                        &origEntryIndex);
    if (result != SOTA_SWAP_MANAGER_RESULT_OK)
    {
        g_sotaSwapDiagDebugLastResult = result;
        return result;
    }
    g_sotaSwapDiagDebugLastOrigEntryIndex = origEntryIndex;

    result = SotaSwapDiag_FindFreeEntry(SOTA_SWAPDIAG_UCB_SWAP_COPY_INDEX,
                                        &copyEntryIndex);
    if (result != SOTA_SWAP_MANAGER_RESULT_OK)
    {
        g_sotaSwapDiagDebugLastResult = result;
        return result;
    }
    g_sotaSwapDiagDebugLastCopyEntryIndex = copyEntryIndex;

    if (origEntryIndex != copyEntryIndex)
    {
        g_sotaSwapDiagDebugLastResult = SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
        return SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
    }

    result = SotaSwapDiag_ProgramEntry(SOTA_SWAPDIAG_UCB_SWAP_ORIG_INDEX,
                                       origEntryIndex,
                                       mode);
    if (result != SOTA_SWAP_MANAGER_RESULT_OK)
    {
        g_sotaSwapDiagDebugLastResult = result;
        return result;
    }

    result = SotaSwapDiag_ProgramEntry(SOTA_SWAPDIAG_UCB_SWAP_COPY_INDEX,
                                       copyEntryIndex,
                                       mode);
    if (result != SOTA_SWAP_MANAGER_RESULT_OK)
    {
        g_sotaSwapDiagDebugLastResult = result;
        return result;
    }

    *outEntryIndex = origEntryIndex;
    g_sotaSwapDiagDebugLastResult = SOTA_SWAP_MANAGER_RESULT_OK;
    return SOTA_SWAP_MANAGER_RESULT_OK;
}

SotaSwapManager_Result_t SotaSwapDiag_FactoryEnableOtp(void)
{
    return SOTA_SWAP_MANAGER_RESULT_NOT_CONFIGURED;
}

SotaSwapManager_Result_t SotaSwapDiag_FactoryEraseAndReinitSwap(void)
{
    return SOTA_SWAP_MANAGER_RESULT_NOT_CONFIGURED;
}

static SotaSwapManager_Result_t SotaSwapDiag_MapFlashResult(SotaFlash_Result_t result)
{
    SotaSwapManager_Result_t mappedResult;

    switch (result)
    {
    case SOTA_FLASH_RESULT_OK:
        mappedResult = SOTA_SWAP_MANAGER_RESULT_OK;
        break;

    case SOTA_FLASH_RESULT_INVALID_PARAM:
    case SOTA_FLASH_RESULT_INVALID_ADDRESS:
    case SOTA_FLASH_RESULT_INVALID_LENGTH:
    case SOTA_FLASH_RESULT_INVALID_RANGE:
    case SOTA_FLASH_RESULT_ACTIVE_BANK_ACCESS:
    case SOTA_FLASH_RESULT_UNSUPPORTED:
        mappedResult = SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
        break;

    case SOTA_FLASH_RESULT_VERIFY_ERROR:
        mappedResult = SOTA_SWAP_MANAGER_RESULT_VERIFY_ERROR;
        break;

    default:
        mappedResult = SOTA_SWAP_MANAGER_RESULT_WRITE_ERROR;
        break;
    }

    return mappedResult;
}

static uint32 SotaSwapDiag_GetEntryOffset(uint8 entryIndex)
{
    return ((uint32)entryIndex * SOTA_SWAPDIAG_ENTRY_SIZE_BYTE);
}

static uint32 SotaSwapDiag_GetEntryMarkerOffset(uint8 entryIndex)
{
    return SotaSwapDiag_GetEntryOffset(entryIndex) +
           SOTA_SWAPDIAG_ENTRY_MARKER_OFFSET;
}

static uint32 SotaSwapDiag_GetTargetMode(uint8 targetBank)
{
    uint32 mode;

    switch ((SotaTc37x_PflashBank_t)targetBank)
    {
    case SOTA_TC37X_PFLASH_BANK_PF0:
        mode = SOTA_SWAPDIAG_MODE_PF0_ACTIVE;
        break;

    case SOTA_TC37X_PFLASH_BANK_PF1:
        mode = SOTA_SWAPDIAG_MODE_PF1_ACTIVE;
        break;

    default:
        mode = 0U;
        break;
    }

    return mode;
}

static boolean SotaSwapDiag_IsValidMode(uint32 mode)
{
    return ((mode == SOTA_SWAPDIAG_MODE_PF0_ACTIVE) ||
            (mode == SOTA_SWAPDIAG_MODE_PF1_ACTIVE)) ? TRUE : FALSE;
}

static boolean SotaSwapDiag_AreWordsAll(const uint32 *words, uint32 value)
{
    uint8 index;

    if (words == NULL_PTR)
    {
        return FALSE;
    }

    for (index = 0U; index < 4U; index++)
    {
        if (words[index] != value)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static boolean SotaSwapDiag_IsEntryErased(const SotaSwapDiag_EntryWords_t words)
{
    return ((SotaSwapDiag_AreWordsAll(words, 0U) == TRUE) ||
            (SotaSwapDiag_AreWordsAll(words, 0xFFFFFFFFU) == TRUE)) ? TRUE : FALSE;
}

static boolean SotaSwapDiag_IsEntryForMode(uint8 ucbIndex,
                                           uint8 entryIndex,
                                           uint32 mode,
                                           const SotaSwapDiag_EntryWords_t words)
{
    uint32 ucbBaseAddress;

    if (SotaFlash_GetUcbStart(ucbIndex, &ucbBaseAddress) !=
        SOTA_FLASH_RESULT_OK)
    {
        return FALSE;
    }

    if (words[0] != mode)
    {
        return FALSE;
    }

    if (words[1] != (ucbBaseAddress + SotaSwapDiag_GetEntryOffset(entryIndex)))
    {
        return FALSE;
    }

    if (words[2] != SOTA_SWAPDIAG_ENTRY_CONFIRM_WORD)
    {
        return FALSE;
    }

    if (words[3] != (ucbBaseAddress +
                     SotaSwapDiag_GetEntryMarkerOffset(entryIndex)))
    {
        return FALSE;
    }

    return TRUE;
}

static SotaSwapManager_Result_t SotaSwapDiag_ReadEntry(uint8 ucbIndex,
                                                       uint8 entryIndex,
                                                       SotaSwapDiag_EntryWords_t words)
{
    SotaFlash_Result_t flashResult;

    if ((entryIndex >= SOTA_SWAPDIAG_ENTRY_COUNT) || (words == NULL_PTR))
    {
        return SOTA_SWAP_MANAGER_RESULT_INVALID_PARAM;
    }

    flashResult = SotaFlash_ReadUcb(ucbIndex,
                                    SotaSwapDiag_GetEntryOffset(entryIndex),
                                    (uint8 *)words,
                                    sizeof(SotaSwapDiag_EntryWords_t));

    return SotaSwapDiag_MapFlashResult(flashResult);
}

static SotaSwapManager_Result_t SotaSwapDiag_VerifyConfirmWord(uint8 ucbIndex)
{
    uint32 confirmWords[2];
    SotaFlash_Result_t flashResult;

    flashResult = SotaFlash_ReadUcb(ucbIndex,
                                    SOTA_SWAPDIAG_UCB_CONFIRM_OFFSET,
                                    (uint8 *)confirmWords,
                                    sizeof(confirmWords));
    if (flashResult != SOTA_FLASH_RESULT_OK)
    {
        return SotaSwapDiag_MapFlashResult(flashResult);
    }

    return (confirmWords[0] == SOTA_SWAPDIAG_UCB_CONFIRM_WORD) ?
           SOTA_SWAP_MANAGER_RESULT_OK :
           SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
}

static SotaSwapManager_Result_t SotaSwapDiag_FindFreeEntry(uint8 ucbIndex,
                                                           uint8 *outEntryIndex)
{
    uint8 entryIndex;
    SotaSwapDiag_EntryWords_t words;
    SotaSwapManager_Result_t result;

    if (outEntryIndex == NULL_PTR)
    {
        return SOTA_SWAP_MANAGER_RESULT_INVALID_PARAM;
    }

    *outEntryIndex = 0xFFU;

    for (entryIndex = 0U; entryIndex < SOTA_SWAPDIAG_ENTRY_COUNT; entryIndex++)
    {
        result = SotaSwapDiag_ReadEntry(ucbIndex, entryIndex, words);
        if (result != SOTA_SWAP_MANAGER_RESULT_OK)
        {
            return result;
        }

        if ((SotaSwapDiag_IsEntryForMode(ucbIndex,
                                         entryIndex,
                                         SOTA_SWAPDIAG_MODE_PF0_ACTIVE,
                                         words) == TRUE) ||
            (SotaSwapDiag_IsEntryForMode(ucbIndex,
                                         entryIndex,
                                         SOTA_SWAPDIAG_MODE_PF1_ACTIVE,
                                         words) == TRUE))
        {
            continue;
        }

        if (SotaSwapDiag_IsEntryErased(words) == FALSE)
        {
            return SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
        }

        *outEntryIndex = entryIndex;
        return SOTA_SWAP_MANAGER_RESULT_OK;
    }

    return SOTA_SWAP_MANAGER_RESULT_NO_FREE_ENTRY;
}

static SotaSwapManager_Result_t SotaSwapDiag_ProgramPageWords(uint8 ucbIndex,
                                                              uint32 offset,
                                                              const uint32 *words)
{
    uint8 pageData[8];
    SotaFlash_Result_t flashResult;

    if (words == NULL_PTR)
    {
        return SOTA_SWAP_MANAGER_RESULT_INVALID_PARAM;
    }

    (void)memcpy(&pageData[0], &words[0], sizeof(uint32));
    (void)memcpy(&pageData[sizeof(uint32)], &words[1], sizeof(uint32));

    flashResult = SotaFlash_ProgramUcbSwapPage8(ucbIndex, offset, pageData);

    return SotaSwapDiag_MapFlashResult(flashResult);
}

static SotaSwapManager_Result_t SotaSwapDiag_ProgramEntry(uint8 ucbIndex,
                                                          uint8 entryIndex,
                                                          uint32 mode)
{
    uint32 ucbBaseAddress;
    uint32 firstPageWords[2];
    uint32 markerPageWords[2];
    SotaSwapDiag_EntryWords_t currentWords;
    SotaSwapManager_Result_t result;

    if ((entryIndex >= SOTA_SWAPDIAG_ENTRY_COUNT) ||
        (SotaSwapDiag_IsValidMode(mode) == FALSE))
    {
        return SOTA_SWAP_MANAGER_RESULT_INVALID_PARAM;
    }

    if (SotaFlash_GetUcbStart(ucbIndex, &ucbBaseAddress) !=
        SOTA_FLASH_RESULT_OK)
    {
        return SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
    }

    result = SotaSwapDiag_ReadEntry(ucbIndex, entryIndex, currentWords);
    if (result != SOTA_SWAP_MANAGER_RESULT_OK)
    {
        return result;
    }

    if (SotaSwapDiag_IsEntryForMode(ucbIndex,
                                    entryIndex,
                                    mode,
                                    currentWords) == TRUE)
    {
        return SOTA_SWAP_MANAGER_RESULT_OK;
    }

    if (SotaSwapDiag_IsEntryErased(currentWords) == FALSE)
    {
        return SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
    }

    firstPageWords[0] = mode;
    firstPageWords[1] = ucbBaseAddress + SotaSwapDiag_GetEntryOffset(entryIndex);
    markerPageWords[0] = SOTA_SWAPDIAG_ENTRY_CONFIRM_WORD;
    markerPageWords[1] = ucbBaseAddress +
                         SotaSwapDiag_GetEntryMarkerOffset(entryIndex);

    result = SotaSwapDiag_ProgramPageWords(ucbIndex,
                                           SotaSwapDiag_GetEntryOffset(entryIndex),
                                           firstPageWords);
    if (result != SOTA_SWAP_MANAGER_RESULT_OK)
    {
        return result;
    }

    result = SotaSwapDiag_ProgramPageWords(ucbIndex,
                                           SotaSwapDiag_GetEntryMarkerOffset(entryIndex),
                                           markerPageWords);
    if (result != SOTA_SWAP_MANAGER_RESULT_OK)
    {
        return result;
    }

    result = SotaSwapDiag_ReadEntry(ucbIndex, entryIndex, currentWords);
    if (result != SOTA_SWAP_MANAGER_RESULT_OK)
    {
        return result;
    }

    return (SotaSwapDiag_IsEntryForMode(ucbIndex,
                                        entryIndex,
                                        mode,
                                        currentWords) == TRUE) ?
           SOTA_SWAP_MANAGER_RESULT_OK :
           SOTA_SWAP_MANAGER_RESULT_VERIFY_ERROR;
}
