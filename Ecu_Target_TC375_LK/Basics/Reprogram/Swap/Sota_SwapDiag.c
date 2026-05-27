/*
 * Sota_SwapDiag.c
 *
 * Read-only TC37x swap diagnostics plus explicitly armed initial provisioning.
 * Provisioning is disabled by default and must not erase UCBs or reset the MCU.
 */

/******************************************************************************/
/*----------------------------------Includes----------------------------------*/
/******************************************************************************/
#include <Sota/Swap/Sota_SwapDiag.h>

#include <IfxDmu_reg.h>
#include <IfxScu_reg.h>
#include <string.h>
#include <Sota/Flash/Sota_FlashTc37x.h>

/******************************************************************************/
/*-------------------------Function Implementations---------------------------*/
/******************************************************************************/
#define SOTA_PROVISION_SWAP_ENTRY0_OFFSET       (0x000u)
#define SOTA_PROVISION_SWAP_CONFIRM0_OFFSET     (0x008u)
#define SOTA_PROVISION_SWAP_ENTRY_SIZE          (0x010u)
#define SOTA_PROVISION_SWAP_ENTRY_COUNT         (16u)
#define SOTA_PROVISION_SWAP_ENTRY_INDEX_INVALID (0xFFFFFFFFu)
#define SOTA_PROVISION_UCB_PROCONTP_OFFSET      (0x1E8u)
#define SOTA_PROVISION_UCB_CONFIRM_OFFSET       (0x1F0u)

volatile uint32 g_sotaProvisionStage = 0u;
volatile uint8 g_sotaProvisionLastUcbNo = 0u;
volatile uint32 g_sotaProvisionLastBaseAddr = 0u;
volatile uint32 g_sotaProvisionLastWords[4] = {0u, 0u, 0u, 0u};

static void SotaProvision_SetOtpDebug(uint32 stage, uint8 ucbNo, uint32 baseAddr, const uint32 words[4])
{
    uint32 index;

    g_sotaProvisionStage = stage;
    g_sotaProvisionLastUcbNo = ucbNo;
    g_sotaProvisionLastBaseAddr = baseAddr;

    if (words == NULL_PTR)
    {
        for (index = 0u; index < 4u; index++)
        {
            g_sotaProvisionLastWords[index] = 0u;
        }
    }
    else
    {
        for (index = 0u; index < 4u; index++)
        {
            g_sotaProvisionLastWords[index] = words[index];
        }
    }
}

static uint32 SotaProvision_GetOtpCheckStage(uint8 ucbNo)
{
    if (ucbNo == UCB_OTP_COPY_NO)
    {
        return SOTA_PROV_STAGE_OTP_CHECK_COPY;
    }

    return SOTA_PROV_STAGE_OTP_CHECK_ORIG;
}

static uint32 SotaProvision_GetOtpEnableStage(uint8 ucbNo)
{
    if (ucbNo == UCB_OTP_COPY_NO)
    {
        return SOTA_PROV_STAGE_OTP_ENABLE_COPY;
    }

    return SOTA_PROV_STAGE_OTP_ENABLE_ORIG;
}

static void SotaSwap_ClearUcbHeadDump(volatile SotaSwap_UcbHeadDump_t *dump)
{
    uint32 index;

    if (dump == NULL_PTR)
    {
        return;
    }

    dump->baseAddr = 0u;

    for (index = 0u; index < 8u; index++)
    {
        dump->word[index] = 0u;
    }

    dump->readOk = 0u;
}

static void SotaSwap_ReadOneUcbHead(volatile SotaSwap_UcbHeadDump_t *dump, uint8 ucbNo)
{
    uint32 head[8];
    uint32 index;
    uint8 readResult;

    if (dump == NULL_PTR)
    {
        return;
    }

    SotaSwap_ClearUcbHeadDump(dump);

    if (ucbNo >= IFXFLASH_DFLASH_NUM_UCB_LOG_SECTORS)
    {
        return;
    }

    dump->baseAddr = IfxFlash_dFlashTableUcbLog[ucbNo].start;

    for (index = 0u; index < 8u; index++)
    {
        head[index] = 0u;
    }

    readResult = SotaFlash_ReadUcb(0u, (uint8 *)&head[0], ucbNo, (uint32)sizeof(head));
    if (readResult != FLASH_RESULT_OK)
    {
        return;
    }

    for (index = 0u; index < 8u; index++)
    {
        dump->word[index] = head[index];
    }

    dump->readOk = 1u;
}

static boolean SotaProvision_GetUcbBase(uint8 ucbNo, uint32 *baseAddr)
{
    if (baseAddr == NULL_PTR)
    {
        return FALSE;
    }

    if (ucbNo >= IFXFLASH_DFLASH_NUM_UCB_LOG_SECTORS)
    {
        *baseAddr = 0u;
        return FALSE;
    }

    *baseAddr = IfxFlash_dFlashTableUcbLog[ucbNo].start;
    return TRUE;
}

static boolean SotaProvision_ReadUcbWords(uint8 ucbNo, uint32 offset, uint32 *words, uint32 wordCount)
{
    uint32 baseAddr;
    uint32 endAddr;
    uint32 byteCount;
    uint32 readAddr;

    if ((words == NULL_PTR) || (wordCount == 0u) || (wordCount > (0xFFFFFFFFu / 4u)))
    {
        return FALSE;
    }

    if (SotaProvision_GetUcbBase(ucbNo, &baseAddr) == FALSE)
    {
        return FALSE;
    }

    endAddr = IfxFlash_dFlashTableUcbLog[ucbNo].end;
    byteCount = wordCount * 4u;

    if ((offset > (0xFFFFFFFFu - byteCount)) ||
        (baseAddr > (0xFFFFFFFFu - offset)))
    {
        return FALSE;
    }

    readAddr = baseAddr + offset;
    if ((readAddr < baseAddr) || ((readAddr + byteCount - 1u) > endAddr))
    {
        return FALSE;
    }

    memcpy(words, (const uint8 *)readAddr, byteCount);
    return TRUE;
}

static boolean SotaProvision_AreWordsAll(const uint32 *words, uint32 wordCount, uint32 value)
{
    uint32 index;

    if ((words == NULL_PTR) || (wordCount == 0u))
    {
        return FALSE;
    }

    for (index = 0u; index < wordCount; index++)
    {
        if (words[index] != value)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static boolean SotaProvision_AreWordsErased(const uint32 *words, uint32 wordCount)
{
    return (boolean)((SotaProvision_AreWordsAll(words, wordCount, 0u) != FALSE) ||
                     (SotaProvision_AreWordsAll(words, wordCount, 0xFFFFFFFFu) != FALSE));
}

static boolean SotaProvision_IsWordEmptyOrValue(uint32 word, uint32 value)
{
    return (boolean)((word == 0x00000000u) ||
                     (word == 0xFFFFFFFFu) ||
                     (word == value));
}

static boolean SotaProvision_IsOtpConfirmWordAllowed(uint32 word)
{
    return (boolean)((word == 0x00000000u) ||
                     (word == SOTA_UCB_UNLOCKED_CODE) ||
                     (word == SOTA_UCB_CONFIRM_CODE));
}

static SotaProvision_Result SotaProvision_ProgramAndVerifyPage(uint32 pageAddr, const uint32 data[2])
{
    uint32 verify[2];
    uint8 flashResult;

    flashResult = SotaFlash_ProgramDflashPage8(pageAddr, data);
    if (flashResult != FLASH_RESULT_OK)
    {
        return SOTA_PROVISION_WRITE_FAILED;
    }

    memcpy(verify, (const uint8 *)pageAddr, sizeof(verify));
    if ((verify[0] != data[0]) || (verify[1] != data[1]))
    {
        return SOTA_PROVISION_VERIFY_FAILED;
    }

    return SOTA_PROVISION_OK;
}

static boolean SotaProvision_IsSwapEntry0WordsStandard(uint8 ucbNo)
{
    uint32 baseAddr;
    uint32 entryWords[4];

    if (SotaProvision_GetUcbBase(ucbNo, &baseAddr) == FALSE)
    {
        return FALSE;
    }

    if (SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_SWAP_ENTRY0_OFFSET, entryWords, 4u) == FALSE)
    {
        return FALSE;
    }

    return (boolean)((entryWords[0] == SOTA_UCB_SWAP_STANDARD) &&
                     (entryWords[1] == (baseAddr + SOTA_PROVISION_SWAP_ENTRY0_OFFSET)) &&
                     (entryWords[2] == SOTA_UCB_CONFIRM_CODE) &&
                     (entryWords[3] == (baseAddr + SOTA_PROVISION_SWAP_CONFIRM0_OFFSET)));
}

static boolean SotaProvision_IsSwapEntry0Standard(uint8 ucbNo)
{
    uint32 unlockWords[2];

    if (SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_UCB_CONFIRM_OFFSET, unlockWords, 2u) == FALSE)
    {
        return FALSE;
    }

    return (boolean)((SotaProvision_IsSwapEntry0WordsStandard(ucbNo) != FALSE) &&
                     (unlockWords[0] == SOTA_UCB_UNLOCKED_CODE));
}

static boolean SotaProvision_IsValidSwapModeWord(uint32 modeWord)
{
    return (boolean)((modeWord == SOTA_UCB_SWAP_STANDARD) ||
                     (modeWord == SOTA_UCB_SWAP_ALTERNATE));
}

static boolean SotaProvision_GetSwapEntryOffsets(uint32 entryIndex,
                                                 uint32 *entryOffset,
                                                 uint32 *confirmOffset)
{
    uint32 offset;

    if ((entryOffset == NULL_PTR) ||
        (confirmOffset == NULL_PTR) ||
        (entryIndex >= SOTA_PROVISION_SWAP_ENTRY_COUNT))
    {
        return FALSE;
    }

    offset = entryIndex * SOTA_PROVISION_SWAP_ENTRY_SIZE;
    *entryOffset = offset;
    *confirmOffset = offset + SOTA_PROVISION_SWAP_CONFIRM0_OFFSET;
    return TRUE;
}

static boolean SotaProvision_ReadSwapEntryWords(uint8 ucbNo, uint32 entryIndex, uint32 words[4])
{
    uint32 entryOffset;
    uint32 confirmOffset;

    if ((words == NULL_PTR) ||
        (SotaProvision_GetSwapEntryOffsets(entryIndex, &entryOffset, &confirmOffset) == FALSE))
    {
        return FALSE;
    }

    return SotaProvision_ReadUcbWords(ucbNo, entryOffset, words, 4u);
}

static boolean SotaProvision_IsSwapEntryWordsForMode(uint8 ucbNo,
                                                     uint32 entryIndex,
                                                     uint32 modeWord)
{
    uint32 baseAddr;
    uint32 entryOffset;
    uint32 confirmOffset;
    uint32 words[4];

    if ((SotaProvision_IsValidSwapModeWord(modeWord) == FALSE) ||
        (SotaProvision_GetUcbBase(ucbNo, &baseAddr) == FALSE) ||
        (SotaProvision_GetSwapEntryOffsets(entryIndex, &entryOffset, &confirmOffset) == FALSE) ||
        (SotaProvision_ReadSwapEntryWords(ucbNo, entryIndex, words) == FALSE))
    {
        return FALSE;
    }

    return (boolean)((words[0] == modeWord) &&
                     (words[1] == (baseAddr + entryOffset)) &&
                     (words[2] == SOTA_UCB_CONFIRM_CODE) &&
                     (words[3] == (baseAddr + confirmOffset)));
}

static boolean SotaProvision_IsSwapEntryUsed(uint8 ucbNo, uint32 entryIndex)
{
    return (boolean)((SotaProvision_IsSwapEntryWordsForMode(ucbNo, entryIndex, SOTA_UCB_SWAP_STANDARD) != FALSE) ||
                     (SotaProvision_IsSwapEntryWordsForMode(ucbNo, entryIndex, SOTA_UCB_SWAP_ALTERNATE) != FALSE));
}

static boolean SotaProvision_IsSwapEntryErased(uint8 ucbNo, uint32 entryIndex)
{
    uint32 words[4];

    if (SotaProvision_ReadSwapEntryWords(ucbNo, entryIndex, words) == FALSE)
    {
        return FALSE;
    }

    return SotaProvision_AreWordsErased(words, 4u);
}

static SotaProvision_Result SotaProvision_FindNextSwapEntry(uint8 ucbNo, uint32 *entryIndex)
{
    uint32 index;

    if (entryIndex == NULL_PTR)
    {
        return SOTA_PROVISION_INVALID_UCB;
    }

    *entryIndex = SOTA_PROVISION_SWAP_ENTRY_INDEX_INVALID;

    for (index = 0u; index < SOTA_PROVISION_SWAP_ENTRY_COUNT; index++)
    {
        if (SotaProvision_IsSwapEntryUsed(ucbNo, index) != FALSE)
        {
            continue;
        }

        if (SotaProvision_IsSwapEntryErased(ucbNo, index) != FALSE)
        {
            *entryIndex = index;
            return SOTA_PROVISION_OK;
        }

        return SOTA_PROVISION_SWAP_ENTRY_INVALID;
    }

    return SOTA_PROVISION_NO_FREE_SWAP_ENTRY;
}

static SotaProvision_Result SotaProvision_ProgramSwapEntryOne(uint8 ucbNo,
                                                              uint32 entryIndex,
                                                              uint32 modeWord)
{
    uint32 baseAddr;
    uint32 entryOffset;
    uint32 confirmOffset;
    uint32 unlockWords[2];
    uint32 pageData[2];
    SotaProvision_Result result;

    if ((SotaProvision_IsValidSwapModeWord(modeWord) == FALSE) ||
        (SotaProvision_GetUcbBase(ucbNo, &baseAddr) == FALSE) ||
        (SotaProvision_GetSwapEntryOffsets(entryIndex, &entryOffset, &confirmOffset) == FALSE))
    {
        return SOTA_PROVISION_INVALID_UCB;
    }

    if (SotaProvision_IsSwapEntryWordsForMode(ucbNo, entryIndex, modeWord) != FALSE)
    {
        return SOTA_PROVISION_OK;
    }

    if (SotaProvision_IsSwapEntryErased(ucbNo, entryIndex) == FALSE)
    {
        return SOTA_PROVISION_SWAP_ENTRY_INVALID;
    }

    if (SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_UCB_CONFIRM_OFFSET, unlockWords, 2u) == FALSE)
    {
        return SOTA_PROVISION_INVALID_UCB;
    }

    if (unlockWords[0] != SOTA_UCB_UNLOCKED_CODE)
    {
        return SOTA_PROVISION_SWAP_ENTRY_INVALID;
    }

    pageData[0] = modeWord;
    pageData[1] = baseAddr + entryOffset;
    result = SotaProvision_ProgramAndVerifyPage(baseAddr + entryOffset, pageData);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    pageData[0] = SOTA_UCB_CONFIRM_CODE;
    pageData[1] = baseAddr + confirmOffset;
    result = SotaProvision_ProgramAndVerifyPage(baseAddr + confirmOffset, pageData);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    if (SotaProvision_IsSwapEntryWordsForMode(ucbNo, entryIndex, modeWord) == FALSE)
    {
        return SOTA_PROVISION_VERIFY_FAILED;
    }

    return SOTA_PROVISION_OK;
}

static SotaProvision_Result SotaProvision_CheckSwapEntry0Writable(uint8 ucbNo)
{
    uint32 entryWords[4];
    uint32 unlockWords[2];

    if ((SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_SWAP_ENTRY0_OFFSET, entryWords, 4u) == FALSE) ||
        (SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_UCB_CONFIRM_OFFSET, unlockWords, 2u) == FALSE))
    {
        return SOTA_PROVISION_INVALID_UCB;
    }

    if ((SotaProvision_IsSwapEntry0WordsStandard(ucbNo) == FALSE) &&
        (SotaProvision_AreWordsErased(entryWords, 4u) == FALSE))
    {
        return SOTA_PROVISION_SWAP_ENTRY_INVALID;
    }

    if (SotaProvision_IsWordEmptyOrValue(unlockWords[0], SOTA_UCB_UNLOCKED_CODE) == FALSE)
    {
        return SOTA_PROVISION_SWAP_ENTRY_INVALID;
    }

    return SOTA_PROVISION_OK;
}

static SotaProvision_Result SotaProvision_ProgramSwapEntry0One(uint8 ucbNo)
{
    uint32 baseAddr;
    uint32 entryWords[4];
    uint32 unlockWords[2];
    uint32 pageData[2];
    SotaProvision_Result result;

    if (SotaProvision_GetUcbBase(ucbNo, &baseAddr) == FALSE)
    {
        return SOTA_PROVISION_INVALID_UCB;
    }

    result = SotaProvision_CheckSwapEntry0Writable(ucbNo);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    if (SotaProvision_IsSwapEntry0Standard(ucbNo) != FALSE)
    {
        return SOTA_PROVISION_OK;
    }

    if (SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_SWAP_ENTRY0_OFFSET, entryWords, 4u) == FALSE)
    {
        return SOTA_PROVISION_INVALID_UCB;
    }

    if (SotaProvision_AreWordsErased(entryWords, 4u) != FALSE)
    {
        pageData[0] = SOTA_UCB_SWAP_STANDARD;
        pageData[1] = baseAddr + SOTA_PROVISION_SWAP_ENTRY0_OFFSET;
        result = SotaProvision_ProgramAndVerifyPage(baseAddr + SOTA_PROVISION_SWAP_ENTRY0_OFFSET, pageData);
        if (result != SOTA_PROVISION_OK)
        {
            return result;
        }

        pageData[0] = SOTA_UCB_CONFIRM_CODE;
        pageData[1] = baseAddr + SOTA_PROVISION_SWAP_CONFIRM0_OFFSET;
        result = SotaProvision_ProgramAndVerifyPage(baseAddr + SOTA_PROVISION_SWAP_CONFIRM0_OFFSET, pageData);
        if (result != SOTA_PROVISION_OK)
        {
            return result;
        }
    }

    if (SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_UCB_CONFIRM_OFFSET, unlockWords, 2u) == FALSE)
    {
        return SOTA_PROVISION_INVALID_UCB;
    }

    if (unlockWords[0] != SOTA_UCB_UNLOCKED_CODE)
    {
        if (SotaProvision_IsWordEmptyOrValue(unlockWords[0], SOTA_UCB_UNLOCKED_CODE) == FALSE)
        {
            return SOTA_PROVISION_SWAP_ENTRY_INVALID;
        }

        pageData[0] = SOTA_UCB_UNLOCKED_CODE;
        pageData[1] = unlockWords[1];
        result = SotaProvision_ProgramAndVerifyPage(baseAddr + SOTA_PROVISION_UCB_CONFIRM_OFFSET, pageData);
        if (result != SOTA_PROVISION_OK)
        {
            return result;
        }
    }

    if (SotaProvision_IsSwapEntry0Standard(ucbNo) == FALSE)
    {
        return SOTA_PROVISION_VERIFY_FAILED;
    }

    return SOTA_PROVISION_OK;
}

static boolean SotaProvision_IsOtpSotaEnabled(uint8 ucbNo)
{
    uint32 procontpWords[2];
    uint32 confirmWords[2];

    if ((SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_UCB_PROCONTP_OFFSET, procontpWords, 2u) == FALSE) ||
        (SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_UCB_CONFIRM_OFFSET, confirmWords, 2u) == FALSE))
    {
        return FALSE;
    }

    return (boolean)(((procontpWords[0] & SOTA_TC37X_PROCONTP_SOTA_MASK) == SOTA_TC37X_PROCONTP_SOTA_MASK) &&
                     (confirmWords[0] == SOTA_UCB_CONFIRM_CODE));
}

static SotaProvision_Result SotaProvision_CheckOtpWritable(uint8 ucbNo)
{
    uint32 baseAddr;
    uint32 procontpWords[2];
    uint32 confirmWords[2];
    uint32 otpWords[4];
    uint32 checkStage;
    boolean procontpSotaEnabled;

    if (SotaProvision_GetUcbBase(ucbNo, &baseAddr) == FALSE)
    {
        return SOTA_PROVISION_INVALID_UCB;
    }

    checkStage = SotaProvision_GetOtpCheckStage(ucbNo);

    if ((SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_UCB_PROCONTP_OFFSET, procontpWords, 2u) == FALSE) ||
        (SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_UCB_CONFIRM_OFFSET, confirmWords, 2u) == FALSE))
    {
        SotaProvision_SetOtpDebug(checkStage, ucbNo, baseAddr, NULL_PTR);
        return SOTA_PROVISION_INVALID_UCB;
    }

    otpWords[0] = procontpWords[0];
    otpWords[1] = procontpWords[1];
    otpWords[2] = confirmWords[0];
    otpWords[3] = confirmWords[1];
    SotaProvision_SetOtpDebug(checkStage, ucbNo, baseAddr, otpWords);

    procontpSotaEnabled = (boolean)((otpWords[0] & SOTA_TC37X_PROCONTP_SOTA_MASK) ==
                                    SOTA_TC37X_PROCONTP_SOTA_MASK);

    if ((otpWords[0] != 0u) && (procontpSotaEnabled == FALSE))
    {
        SotaProvision_SetOtpDebug(SOTA_PROV_STAGE_OTP_PROCONTP_INVALID, ucbNo, baseAddr, otpWords);
        return SOTA_PROVISION_OTP_INVALID;
    }

    if (otpWords[1] != 0u)
    {
        SotaProvision_SetOtpDebug(SOTA_PROV_STAGE_OTP_PROCONTP_PAIR_USED, ucbNo, baseAddr, otpWords);
        return SOTA_PROVISION_OTP_INVALID;
    }

    if (SotaProvision_IsOtpConfirmWordAllowed(otpWords[2]) == FALSE)
    {
        SotaProvision_SetOtpDebug(SOTA_PROV_STAGE_OTP_CONFIRM_INVALID, ucbNo, baseAddr, otpWords);
        return SOTA_PROVISION_OTP_INVALID;
    }

    if ((otpWords[2] == SOTA_UCB_CONFIRM_CODE) && (procontpSotaEnabled == FALSE))
    {
        SotaProvision_SetOtpDebug(SOTA_PROV_STAGE_OTP_CONFIRM_INVALID, ucbNo, baseAddr, otpWords);
        return SOTA_PROVISION_OTP_INVALID;
    }

    if (otpWords[3] != 0u)
    {
        SotaProvision_SetOtpDebug(SOTA_PROV_STAGE_OTP_CONFIRM_PAIR_USED, ucbNo, baseAddr, otpWords);
        return SOTA_PROVISION_OTP_INVALID;
    }

    return SOTA_PROVISION_OK;
}

static SotaProvision_Result SotaProvision_EnableSotaInOtpOne(uint8 ucbNo)
{
    uint32 baseAddr;
    uint32 procontpWords[2];
    uint32 confirmWords[2];
    uint32 otpWords[4];
    uint32 pageData[2];
    uint32 newProcontp;
    uint32 enableStage;
    SotaProvision_Result result;

    if (SotaProvision_GetUcbBase(ucbNo, &baseAddr) == FALSE)
    {
        return SOTA_PROVISION_INVALID_UCB;
    }

    enableStage = SotaProvision_GetOtpEnableStage(ucbNo);

    result = SotaProvision_CheckOtpWritable(ucbNo);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    SotaProvision_SetOtpDebug(enableStage, ucbNo, baseAddr, NULL_PTR);

    if (SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_UCB_PROCONTP_OFFSET, procontpWords, 2u) == FALSE)
    {
        SotaProvision_SetOtpDebug(enableStage, ucbNo, baseAddr, NULL_PTR);
        return SOTA_PROVISION_INVALID_UCB;
    }

    newProcontp = procontpWords[0] | SOTA_TC37X_PROCONTP_SOTA_MASK;
    if (procontpWords[0] != newProcontp)
    {
        pageData[0] = newProcontp;
        pageData[1] = procontpWords[1];
        result = SotaProvision_ProgramAndVerifyPage(baseAddr + SOTA_PROVISION_UCB_PROCONTP_OFFSET, pageData);
        if (result != SOTA_PROVISION_OK)
        {
            return result;
        }
    }

    if (SotaProvision_ReadUcbWords(ucbNo, SOTA_PROVISION_UCB_CONFIRM_OFFSET, confirmWords, 2u) == FALSE)
    {
        SotaProvision_SetOtpDebug(enableStage, ucbNo, baseAddr, NULL_PTR);
        return SOTA_PROVISION_INVALID_UCB;
    }

    otpWords[0] = newProcontp;
    otpWords[1] = procontpWords[1];
    otpWords[2] = confirmWords[0];
    otpWords[3] = confirmWords[1];
    SotaProvision_SetOtpDebug(enableStage, ucbNo, baseAddr, otpWords);

    if (confirmWords[0] != SOTA_UCB_CONFIRM_CODE)
    {
        if (SotaProvision_IsOtpConfirmWordAllowed(confirmWords[0]) == FALSE)
        {
            SotaProvision_SetOtpDebug(SOTA_PROV_STAGE_OTP_CONFIRM_INVALID, ucbNo, baseAddr, otpWords);
            return SOTA_PROVISION_OTP_INVALID;
        }

        pageData[0] = SOTA_UCB_CONFIRM_CODE;
        pageData[1] = confirmWords[1];
        result = SotaProvision_ProgramAndVerifyPage(baseAddr + SOTA_PROVISION_UCB_CONFIRM_OFFSET, pageData);
        if (result != SOTA_PROVISION_OK)
        {
            return result;
        }
    }

    if (SotaProvision_IsOtpSotaEnabled(ucbNo) == FALSE)
    {
        return SOTA_PROVISION_VERIFY_FAILED;
    }

    return SOTA_PROVISION_OK;
}

uint8 SotaSwap_GetCurrentMode(void)
{
    uint8 ret;

    if (SCU_SWAPCTRL.B.ADDRCFG == 1u)
    {
        ret = (uint8)SOTA_SWAP_STANDARD;
    }
    else if (SCU_SWAPCTRL.B.ADDRCFG == 2u)
    {
        ret = (uint8)SOTA_SWAP_ALTERNATE;
    }
    else
    {
        ret = 0xFFu;
    }

    return ret;
}

uint8 SotaSwap_CheckSwapActive(void)
{
    uint8 ret = 0u;

    if (DMU_HF_PROCONTP.B.SWAPEN == 0x03u)
    {
        ret = 1u;
    }

    return ret;
}

void SotaSwap_MinDiag_Update(volatile SotaSwap_MinDiag_t *diag)
{
    uint32 stmem1;
    uint32 stmem2;

    if (diag == NULL_PTR)
    {
        return;
    }

    stmem1 = SCU_STMEM1.U;
    stmem2 = SCU_STMEM2.U;

    diag->stmem1 = stmem1;
    diag->stmem2 = stmem2;
    diag->swapctrl = SCU_SWAPCTRL.U;
    diag->procontp = DMU_HF_PROCONTP.U;

    diag->swapEn = (uint8)DMU_HF_PROCONTP.B.SWAPEN;
    diag->swapCfg = (uint8)((stmem1 >> 16u) & 0x3u);
    diag->swapTarget = (uint8)((stmem1 >> 18u) & 0x1u);
    diag->swapDwIndex = (uint8)((stmem1 >> 19u) & 0x1Fu);
    diag->swapEntryIndex = (uint8)(diag->swapDwIndex >> 1u);

    diag->bootAddr = stmem2 & 0xFFFFFFFCu;
    diag->currentMode = SotaSwap_GetCurrentMode();
}

void SotaSwap_ReadUcbHeads(volatile SotaSwap_UcbHeadDump_t *otpOrig,
                           volatile SotaSwap_UcbHeadDump_t *otpCopy,
                           volatile SotaSwap_UcbHeadDump_t *swapOrig,
                           volatile SotaSwap_UcbHeadDump_t *swapCopy)
{
    SotaSwap_ReadOneUcbHead(otpOrig, UCB_OTP_ORIG_NO);
    SotaSwap_ReadOneUcbHead(otpCopy, UCB_OTP_COPY_NO);
    SotaSwap_ReadOneUcbHead(swapOrig, UCB_SWAP_ORIG_NO);
    SotaSwap_ReadOneUcbHead(swapCopy, UCB_SWAP_COPY_NO);
}

SotaProvision_Result SotaProvision_ProgramNextSwapEntry(uint32 *entryIndex,
                                                        uint32 *targetModeWord)
{
#if ((SOTA_ENABLE_UCB_WRITES != 0u) && \
     (SOTA_ENABLE_UCB_SWAP_WRITES != 0u))
    uint8 currentMode;
    uint32 targetWord;
    uint32 origEntry;
    uint32 copyEntry;
    SotaProvision_Result result;

    if (entryIndex != NULL_PTR)
    {
        *entryIndex = SOTA_PROVISION_SWAP_ENTRY_INDEX_INVALID;
    }

    if (targetModeWord != NULL_PTR)
    {
        *targetModeWord = 0u;
    }

    if (SotaSwap_CheckSwapActive() == 0u)
    {
        return SOTA_PROVISION_OTP_INVALID;
    }

    currentMode = SotaSwap_GetCurrentMode();
    if (currentMode == (uint8)SOTA_SWAP_STANDARD)
    {
        targetWord = SOTA_UCB_SWAP_ALTERNATE;
    }
    else if (currentMode == (uint8)SOTA_SWAP_ALTERNATE)
    {
        targetWord = SOTA_UCB_SWAP_STANDARD;
    }
    else
    {
        return SOTA_PROVISION_SWAP_ENTRY_INVALID;
    }

    result = SotaProvision_FindNextSwapEntry(UCB_SWAP_ORIG_NO, &origEntry);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    result = SotaProvision_FindNextSwapEntry(UCB_SWAP_COPY_NO, &copyEntry);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    if (origEntry != copyEntry)
    {
        return SOTA_PROVISION_SWAP_ENTRY_INVALID;
    }

    result = SotaProvision_ProgramSwapEntryOne(UCB_SWAP_ORIG_NO, origEntry, targetWord);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    result = SotaProvision_ProgramSwapEntryOne(UCB_SWAP_COPY_NO, copyEntry, targetWord);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    if ((SotaProvision_IsSwapEntryWordsForMode(UCB_SWAP_ORIG_NO, origEntry, targetWord) == FALSE) ||
        (SotaProvision_IsSwapEntryWordsForMode(UCB_SWAP_COPY_NO, copyEntry, targetWord) == FALSE))
    {
        return SOTA_PROVISION_VERIFY_FAILED;
    }

    if (entryIndex != NULL_PTR)
    {
        *entryIndex = origEntry;
    }

    if (targetModeWord != NULL_PTR)
    {
        *targetModeWord = targetWord;
    }

    return SOTA_PROVISION_OK;
#else
    if (entryIndex != NULL_PTR)
    {
        *entryIndex = SOTA_PROVISION_SWAP_ENTRY_INDEX_INVALID;
    }

    if (targetModeWord != NULL_PTR)
    {
        *targetModeWord = 0u;
    }

    return SOTA_PROVISION_DISABLED;
#endif
}

SotaProvision_Result SotaProvision_ProgramSwapEntry0Standard(void)
{
#if ((SOTA_ENABLE_INITIAL_PROVISIONING != 0u) && \
     (SOTA_ENABLE_UCB_WRITES != 0u) && \
     (SOTA_ENABLE_UCB_SWAP_WRITES != 0u))
    SotaProvision_Result result;

    result = SotaProvision_CheckSwapEntry0Writable(UCB_SWAP_ORIG_NO);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    result = SotaProvision_CheckSwapEntry0Writable(UCB_SWAP_COPY_NO);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    result = SotaProvision_ProgramSwapEntry0One(UCB_SWAP_ORIG_NO);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    result = SotaProvision_ProgramSwapEntry0One(UCB_SWAP_COPY_NO);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    return SOTA_PROVISION_OK;
#else
    return SOTA_PROVISION_DISABLED;
#endif
}

SotaProvision_Result SotaProvision_ReinitSwapEntry0Standard(uint32 *origEraseResult,
                                                            uint32 *copyEraseResult)
{
#if ((SOTA_ENABLE_UCB_WRITES != 0u) && \
     (SOTA_ENABLE_UCB_SWAP_WRITES != 0u) && \
     (SOTA_ENABLE_UCB_SWAP_ERASE_REINIT != 0u))
    uint8 flashResult;
    SotaProvision_Result result;

    if (origEraseResult != NULL_PTR)
    {
        *origEraseResult = FLASH_RESULT_UCB_WRITE_DISABLED;
    }

    if (copyEraseResult != NULL_PTR)
    {
        *copyEraseResult = FLASH_RESULT_UCB_WRITE_DISABLED;
    }

    g_sotaProvisionStage = SOTA_PROV_STAGE_SWAP_ERASE_ORIG;
    g_sotaProvisionLastUcbNo = UCB_SWAP_ORIG_NO;
    flashResult = SotaFlash_EraseUcb(0u, UCB_SWAP_ORIG_NO);
    if (origEraseResult != NULL_PTR)
    {
        *origEraseResult = flashResult;
    }

    if (flashResult != FLASH_RESULT_OK)
    {
        return SOTA_PROVISION_WRITE_FAILED;
    }

    g_sotaProvisionStage = SOTA_PROV_STAGE_SWAP_ERASE_COPY;
    g_sotaProvisionLastUcbNo = UCB_SWAP_COPY_NO;
    flashResult = SotaFlash_EraseUcb(0u, UCB_SWAP_COPY_NO);
    if (copyEraseResult != NULL_PTR)
    {
        *copyEraseResult = flashResult;
    }

    if (flashResult != FLASH_RESULT_OK)
    {
        return SOTA_PROVISION_WRITE_FAILED;
    }

    g_sotaProvisionStage = SOTA_PROV_STAGE_SWAP_REWRITE_ORIG;
    result = SotaProvision_ProgramSwapEntry0One(UCB_SWAP_ORIG_NO);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    g_sotaProvisionStage = SOTA_PROV_STAGE_SWAP_REWRITE_COPY;
    result = SotaProvision_ProgramSwapEntry0One(UCB_SWAP_COPY_NO);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    if ((SotaProvision_IsSwapEntry0Standard(UCB_SWAP_ORIG_NO) == FALSE) ||
        (SotaProvision_IsSwapEntry0Standard(UCB_SWAP_COPY_NO) == FALSE))
    {
        return SOTA_PROVISION_VERIFY_FAILED;
    }

    g_sotaProvisionStage = SOTA_PROV_STAGE_DONE;
    return SOTA_PROVISION_OK;
#else
    if (origEraseResult != NULL_PTR)
    {
        *origEraseResult = FLASH_RESULT_UCB_WRITE_DISABLED;
    }

    if (copyEraseResult != NULL_PTR)
    {
        *copyEraseResult = FLASH_RESULT_UCB_WRITE_DISABLED;
    }

    return SOTA_PROVISION_DISABLED;
#endif
}

SotaProvision_Result SotaProvision_EnableSotaInOtp0(void)
{
#if ((SOTA_ENABLE_INITIAL_PROVISIONING != 0u) && \
     (SOTA_ENABLE_UCB_WRITES != 0u) && \
     (SOTA_ENABLE_UCB_OTP_WRITES != 0u))
    SotaProvision_Result result;

    result = SotaProvision_CheckOtpWritable(UCB_OTP_ORIG_NO);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    result = SotaProvision_CheckOtpWritable(UCB_OTP_COPY_NO);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    result = SotaProvision_EnableSotaInOtpOne(UCB_OTP_ORIG_NO);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    result = SotaProvision_EnableSotaInOtpOne(UCB_OTP_COPY_NO);
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    return SOTA_PROVISION_OK;
#else
    return SOTA_PROVISION_DISABLED;
#endif
}

SotaProvision_Result SotaProvision_RunInitialOnce(void)
{
#if (SOTA_ENABLE_INITIAL_PROVISIONING != 0u)
    SotaProvision_Result result;

    if (DMU_HF_PROCONTP.B.SWAPEN == 0x03u)
    {
        return SOTA_PROVISION_ALREADY_ENABLED;
    }

    result = SotaProvision_ProgramSwapEntry0Standard();
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    result = SotaProvision_EnableSotaInOtp0();
    if (result != SOTA_PROVISION_OK)
    {
        return result;
    }

    if ((SotaProvision_IsSwapEntry0Standard(UCB_SWAP_ORIG_NO) == FALSE) ||
        (SotaProvision_IsSwapEntry0Standard(UCB_SWAP_COPY_NO) == FALSE) ||
        (SotaProvision_IsOtpSotaEnabled(UCB_OTP_ORIG_NO) == FALSE) ||
        (SotaProvision_IsOtpSotaEnabled(UCB_OTP_COPY_NO) == FALSE))
    {
        return SOTA_PROVISION_VERIFY_FAILED;
    }

    g_sotaProvisionStage = SOTA_PROV_STAGE_DONE;
    return SOTA_PROVISION_OK;
#else
    return SOTA_PROVISION_DISABLED;
#endif
}
