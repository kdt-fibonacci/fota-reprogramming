#include "Sota_FlashTc37x.h"

#include <string.h>

#include "Cpu/Std/IfxCpu.h"
#include "Flash/Std/IfxFlash.h"
#include "Scu/Std/IfxScuWdt.h"

#define SOTA_FLASH_INVALID_ADDRESS      (0xFFFFFFFFU)
#define SOTA_FLASH_UCB_SWAP_ORIG_INDEX (23U)
#define SOTA_FLASH_UCB_SWAP_COPY_INDEX (31U)

#if defined(__GNUC__)
#define SOTA_FLASH_RAM_CODE __attribute__((section(".cpu0_psram")))
#else
#define SOTA_FLASH_RAM_CODE
#endif

static SotaFlash_Result_t SotaFlash_MapRangeStatus(SotaTc37x_RangeStatus_t status);
static SotaFlash_Bank_t SotaFlash_MapBank(SotaTc37x_PflashBank_t bank);
static IfxFlash_FlashType SotaFlash_GetPflashType(SotaFlash_Bank_t bank);
static boolean SotaFlash_IsAligned(uint32 value, uint32 alignment);
static boolean SotaFlash_IsUcbIndexValid(uint8 ucbIndex);
static boolean SotaFlash_IsUcbRangeValid(uint8 ucbIndex,
                                         uint32 offset,
                                         uint32 length);
static boolean SotaFlash_IsRuntimeSwapUcb(uint8 ucbIndex);
static SotaFlash_Result_t SOTA_FLASH_RAM_CODE SotaFlash_ProgramPage8(uint32 pageAddress,
                                                                     const uint8 *pageData,
                                                                     IfxFlash_FlashType flashType);
static SotaFlash_Result_t SOTA_FLASH_RAM_CODE SotaFlash_ErasePflashSectorCommand(uint32 sectorAddress,
                                                                                 IfxFlash_FlashType flashType);
static SotaFlash_Result_t SOTA_FLASH_RAM_CODE SotaFlash_ProgramPflashPage32(uint32 pageAddress,
                                                                            const uint8 *pageData,
                                                                            IfxFlash_FlashType flashType);
static uint32 SotaFlash_ReadWord32(const void *address);

volatile uint32 g_sotaFlashDebugLastEraseAddress;
volatile uint32 g_sotaFlashDebugLastEraseBank;
volatile uint32 g_sotaFlashDebugLastEraseType;
volatile uint32 g_sotaFlashDebugLastEraseResult;
volatile uint32 g_sotaFlashDebugLastProgramAddress;
volatile uint32 g_sotaFlashDebugLastProgramBank;
volatile uint32 g_sotaFlashDebugLastProgramType;
volatile uint32 g_sotaFlashDebugLastProgramCommandResult;
volatile uint32 g_sotaFlashDebugLastProgramVerifyResult;
volatile uint32 g_sotaFlashDebugLastProgramExpected0;
volatile uint32 g_sotaFlashDebugLastProgramExpected1;
volatile uint32 g_sotaFlashDebugLastProgramActual0;
volatile uint32 g_sotaFlashDebugLastProgramActual1;
volatile uint32 g_sotaFlashDebugDmuErrAfterErase;
volatile uint32 g_sotaFlashDebugDmuErrAfterProgram;

SotaFlash_Bank_t SotaFlash_GetBankByAddress(uint32 address)
{
    return SotaFlash_MapBank(SotaTc37x_GetBankByAddress(address));
}

SotaFlash_Bank_t SotaFlash_GetActiveBank(void)
{
    return SotaFlash_MapBank(SotaTc37x_GetCurrentActiveBank());
}

SotaFlash_Bank_t SotaFlash_GetInactiveBank(void)
{
    return SotaFlash_MapBank(SotaTc37x_GetInactiveBank());
}

uint32 SotaFlash_GetBankStart(SotaFlash_Bank_t bank)
{
    return SotaTc37x_GetBankStart((SotaTc37x_PflashBank_t)bank);
}

uint32 SotaFlash_GetBankEnd(SotaFlash_Bank_t bank)
{
    return SotaTc37x_GetBankEnd((SotaTc37x_PflashBank_t)bank);
}

uint32 SotaFlash_GetBankSize(SotaFlash_Bank_t bank)
{
    return SotaTc37x_GetBankSize((SotaTc37x_PflashBank_t)bank);
}

uint32 SotaFlash_GetInactiveBankStart(void)
{
    return SotaTc37x_GetInactiveBankStart();
}

uint32 SotaFlash_GetInactiveBankEnd(void)
{
    uint32 start = SotaFlash_GetInactiveBankStart();
    uint32 size = SotaFlash_GetInactiveBankSize();

    return ((start == 0U) || (size == 0U)) ? 0U : (start + size - 1U);
}

uint32 SotaFlash_GetInactiveBankSize(void)
{
    return SotaTc37x_GetInactiveBankSize();
}

uint32 SotaFlash_GetPflashPageSize(void)
{
    return SOTA_FLASH_PFLASH_PAGE_SIZE_BYTE;
}

uint32 SotaFlash_GetPflashSectorSize(void)
{
    return SOTA_FLASH_PFLASH_SECTOR_SIZE_BYTE;
}

uint32 SotaFlash_NormalizePflashAddress(uint32 address)
{
    return SotaTc37x_NormalizePflashAddress(address);
}

boolean SotaFlash_IsAddressRangeInsideInactiveBank(uint32 address,
                                                   uint32 length)
{
    return SotaTc37x_IsRangeInsideInactiveBank(address, length);
}

SotaFlash_Result_t SotaFlash_CopyPflashRoutinesToPspr(void)
{
    /*
     * The flash command helpers below are linked into .text.cpu0_psram by the
     * TASKING linker script. Startup copy table initialization moves them to
     * PSPR before runtime use.
     */
    return SOTA_FLASH_RESULT_OK;
}

SotaFlash_Result_t SotaFlash_ValidateInactivePflashRange(uint32 address,
                                                         uint32 length)
{
    return SotaFlash_MapRangeStatus(SotaTc37x_ValidateInactiveBankRange(address,
                                                                        length));
}

SotaFlash_Result_t SotaFlash_EraseSector(uint32 sectorAddress)
{
    SotaFlash_Bank_t bank;
    IfxFlash_FlashType flashType;
    uint32 normalizedAddress;
    SotaFlash_Result_t rangeResult;

    normalizedAddress = SotaFlash_NormalizePflashAddress(sectorAddress);
    if (normalizedAddress == SOTA_FLASH_INVALID_ADDRESS)
    {
        return SOTA_FLASH_RESULT_INVALID_ADDRESS;
    }

    if (SotaFlash_IsAligned(normalizedAddress,
                            SOTA_FLASH_PFLASH_SECTOR_SIZE_BYTE) == FALSE)
    {
        return SOTA_FLASH_RESULT_INVALID_ADDRESS;
    }

    rangeResult = SotaFlash_ValidateInactivePflashRange(
        normalizedAddress,
        SOTA_FLASH_PFLASH_SECTOR_SIZE_BYTE);
    if (rangeResult != SOTA_FLASH_RESULT_OK)
    {
        return rangeResult;
    }

    bank = SotaFlash_GetBankByAddress(normalizedAddress);
    flashType = SotaFlash_GetPflashType(bank);
    if (flashType == IfxFlash_FlashType_Fa)
    {
        return SOTA_FLASH_RESULT_INVALID_ADDRESS;
    }

    (void)SotaFlash_CopyPflashRoutinesToPspr();

    g_sotaFlashDebugLastEraseAddress = normalizedAddress;
    g_sotaFlashDebugLastEraseBank = (uint32)bank;
    g_sotaFlashDebugLastEraseType = (uint32)flashType;
    g_sotaFlashDebugLastEraseResult =
        (uint32)SotaFlash_ErasePflashSectorCommand(normalizedAddress, flashType);
    g_sotaFlashDebugDmuErrAfterErase = MODULE_DMU.HF_ERRSR.U;

    return (SotaFlash_Result_t)g_sotaFlashDebugLastEraseResult;
}

SotaFlash_Result_t SotaFlash_ProgramPage32(uint32 pageAddress,
                                           const uint8 *pageData)
{
    SotaFlash_Bank_t bank;
    IfxFlash_FlashType flashType;
    uint32 normalizedAddress;
    SotaFlash_Result_t result;

    if (pageData == NULL_PTR)
    {
        return SOTA_FLASH_RESULT_INVALID_PARAM;
    }

    normalizedAddress = SotaFlash_NormalizePflashAddress(pageAddress);
    if (normalizedAddress == SOTA_FLASH_INVALID_ADDRESS)
    {
        return SOTA_FLASH_RESULT_INVALID_ADDRESS;
    }

    if (SotaFlash_IsAligned(normalizedAddress,
                            SOTA_FLASH_PFLASH_PAGE_SIZE_BYTE) == FALSE)
    {
        return SOTA_FLASH_RESULT_INVALID_ADDRESS;
    }

    result = SotaFlash_ValidateInactivePflashRange(normalizedAddress,
                                                  SOTA_FLASH_PFLASH_PAGE_SIZE_BYTE);
    if (result != SOTA_FLASH_RESULT_OK)
    {
        return result;
    }

    bank = SotaFlash_GetBankByAddress(normalizedAddress);
    flashType = SotaFlash_GetPflashType(bank);
    if (flashType == IfxFlash_FlashType_Fa)
    {
        return SOTA_FLASH_RESULT_INVALID_ADDRESS;
    }

    (void)SotaFlash_CopyPflashRoutinesToPspr();

    g_sotaFlashDebugLastProgramAddress = normalizedAddress;
    g_sotaFlashDebugLastProgramBank = (uint32)bank;
    g_sotaFlashDebugLastProgramType = (uint32)flashType;
    g_sotaFlashDebugLastProgramExpected0 =
        SotaFlash_ReadWord32((const void *)&pageData[0]);
    g_sotaFlashDebugLastProgramExpected1 =
        SotaFlash_ReadWord32((const void *)&pageData[sizeof(uint32)]);
    g_sotaFlashDebugLastProgramActual0 = 0U;
    g_sotaFlashDebugLastProgramActual1 = 0U;
    g_sotaFlashDebugLastProgramCommandResult = (uint32)SOTA_FLASH_RESULT_OK;
    g_sotaFlashDebugLastProgramVerifyResult = (uint32)SOTA_FLASH_RESULT_OK;

    result = SotaFlash_ProgramPflashPage32(normalizedAddress,
                                          pageData,
                                          flashType);
    g_sotaFlashDebugDmuErrAfterProgram = MODULE_DMU.HF_ERRSR.U;
    g_sotaFlashDebugLastProgramCommandResult = (uint32)result;
    if (result != SOTA_FLASH_RESULT_OK)
    {
        return result;
    }

    g_sotaFlashDebugLastProgramActual0 =
        SotaFlash_ReadWord32((const void *)normalizedAddress);
    g_sotaFlashDebugLastProgramActual1 =
        SotaFlash_ReadWord32((const void *)(normalizedAddress + sizeof(uint32)));

    if (memcmp((const void *)normalizedAddress,
               (const void *)pageData,
               SOTA_FLASH_PFLASH_PAGE_SIZE_BYTE) != 0)
    {
        g_sotaFlashDebugLastProgramVerifyResult =
            (uint32)SOTA_FLASH_RESULT_VERIFY_ERROR;
        return SOTA_FLASH_RESULT_VERIFY_ERROR;
    }

    return SOTA_FLASH_RESULT_OK;
}

SotaFlash_Result_t SotaFlash_Read(uint32 address,
                                  uint8 *outData,
                                  uint32 length)
{
    uint32 normalizedAddress;
    SotaTc37x_PflashBank_t bank;
    uint32 rangeEnd;

    if (outData == NULL_PTR)
    {
        return SOTA_FLASH_RESULT_INVALID_PARAM;
    }

    normalizedAddress = SotaFlash_NormalizePflashAddress(address);
    if (normalizedAddress == SOTA_FLASH_INVALID_ADDRESS)
    {
        return SOTA_FLASH_RESULT_INVALID_ADDRESS;
    }

    if (length == 0U)
    {
        return SOTA_FLASH_RESULT_INVALID_LENGTH;
    }

    rangeEnd = normalizedAddress + length - 1U;
    if (rangeEnd < normalizedAddress)
    {
        return SOTA_FLASH_RESULT_INVALID_LENGTH;
    }

    bank = SotaTc37x_GetBankByAddress(normalizedAddress);
    if ((bank == SOTA_TC37X_PFLASH_BANK_UNKNOWN) ||
        (SotaTc37x_GetBankByAddress(rangeEnd) != bank))
    {
        return SOTA_FLASH_RESULT_INVALID_ADDRESS;
    }

    (void)memcpy(outData, (const void *)normalizedAddress, length);
    return SOTA_FLASH_RESULT_OK;
}

SotaFlash_Result_t SotaFlash_GetUcbStart(uint8 ucbIndex,
                                         uint32 *outStartAddress)
{
    if (outStartAddress == NULL_PTR)
    {
        return SOTA_FLASH_RESULT_INVALID_PARAM;
    }

    if (SotaFlash_IsUcbIndexValid(ucbIndex) == FALSE)
    {
        return SOTA_FLASH_RESULT_INVALID_ADDRESS;
    }

    *outStartAddress = IfxFlash_dFlashTableUcbLog[ucbIndex].start;
    return SOTA_FLASH_RESULT_OK;
}

SotaFlash_Result_t SotaFlash_ReadUcb(uint8 ucbIndex,
                                     uint32 offset,
                                     uint8 *outData,
                                     uint32 length)
{
    uint32 startAddress;

    if (outData == NULL_PTR)
    {
        return SOTA_FLASH_RESULT_INVALID_PARAM;
    }

    if (length == 0U)
    {
        return SOTA_FLASH_RESULT_INVALID_LENGTH;
    }

    if (SotaFlash_IsUcbRangeValid(ucbIndex, offset, length) == FALSE)
    {
        return SOTA_FLASH_RESULT_INVALID_RANGE;
    }

    startAddress = IfxFlash_dFlashTableUcbLog[ucbIndex].start;
    (void)memcpy(outData, (const void *)(startAddress + offset), length);

    return SOTA_FLASH_RESULT_OK;
}

SotaFlash_Result_t SotaFlash_ProgramUcbSwapPage8(uint8 ucbIndex,
                                                 uint32 offset,
                                                 const uint8 *pageData)
{
    uint32 pageAddress;
    SotaFlash_Result_t result;

    if (pageData == NULL_PTR)
    {
        return SOTA_FLASH_RESULT_INVALID_PARAM;
    }

    if (SotaFlash_IsRuntimeSwapUcb(ucbIndex) == FALSE)
    {
        return SOTA_FLASH_RESULT_UNSUPPORTED;
    }

    if ((SotaFlash_IsAligned(offset, IFXFLASH_DFLASH_PAGE_LENGTH) == FALSE) ||
        (SotaFlash_IsUcbRangeValid(ucbIndex,
                                   offset,
                                   IFXFLASH_DFLASH_PAGE_LENGTH) == FALSE))
    {
        return SOTA_FLASH_RESULT_INVALID_RANGE;
    }

    pageAddress = IfxFlash_dFlashTableUcbLog[ucbIndex].start + offset;

    (void)SotaFlash_CopyPflashRoutinesToPspr();

    result = SotaFlash_ProgramPage8(pageAddress,
                                    pageData,
                                    IfxFlash_FlashType_D0);
    if (result != SOTA_FLASH_RESULT_OK)
    {
        return result;
    }

    if (memcmp((const void *)pageAddress,
               (const void *)pageData,
               IFXFLASH_DFLASH_PAGE_LENGTH) != 0)
    {
        return SOTA_FLASH_RESULT_VERIFY_ERROR;
    }

    return SOTA_FLASH_RESULT_OK;
}

static SotaFlash_Result_t SotaFlash_MapRangeStatus(SotaTc37x_RangeStatus_t status)
{
    SotaFlash_Result_t result;

    switch (status)
    {
    case SOTA_TC37X_RANGE_STATUS_OK:
        result = SOTA_FLASH_RESULT_OK;
        break;

    case SOTA_TC37X_RANGE_STATUS_INVALID_ADDRESS:
        result = SOTA_FLASH_RESULT_INVALID_ADDRESS;
        break;

    case SOTA_TC37X_RANGE_STATUS_INVALID_LENGTH:
        result = SOTA_FLASH_RESULT_INVALID_LENGTH;
        break;

    case SOTA_TC37X_RANGE_STATUS_ACTIVE_BANK_ACCESS:
        result = SOTA_FLASH_RESULT_ACTIVE_BANK_ACCESS;
        break;

    default:
        result = SOTA_FLASH_RESULT_INVALID_RANGE;
        break;
    }

    return result;
}

static uint32 SotaFlash_ReadWord32(const void *address)
{
    uint32 value;

    (void)memcpy(&value, address, sizeof(uint32));
    return value;
}

static SotaFlash_Bank_t SotaFlash_MapBank(SotaTc37x_PflashBank_t bank)
{
    SotaFlash_Bank_t mappedBank;

    switch (bank)
    {
    case SOTA_TC37X_PFLASH_BANK_PF0:
        mappedBank = SOTA_FLASH_BANK_PF0;
        break;

    case SOTA_TC37X_PFLASH_BANK_PF1:
        mappedBank = SOTA_FLASH_BANK_PF1;
        break;

    default:
        mappedBank = SOTA_FLASH_BANK_UNKNOWN;
        break;
    }

    return mappedBank;
}

static IfxFlash_FlashType SotaFlash_GetPflashType(SotaFlash_Bank_t bank)
{
    IfxFlash_FlashType flashType;

    switch (bank)
    {
    case SOTA_FLASH_BANK_PF0:
        flashType = IfxFlash_FlashType_P0;
        break;

    case SOTA_FLASH_BANK_PF1:
        flashType = IfxFlash_FlashType_P1;
        break;

    default:
        flashType = IfxFlash_FlashType_Fa;
        break;
    }

    return flashType;
}

static boolean SotaFlash_IsAligned(uint32 value, uint32 alignment)
{
    return ((alignment != 0U) && ((value % alignment) == 0U)) ? TRUE : FALSE;
}

static boolean SotaFlash_IsUcbIndexValid(uint8 ucbIndex)
{
    return (ucbIndex < IFXFLASH_DFLASH_NUM_UCB_LOG_SECTORS) ? TRUE : FALSE;
}

static boolean SotaFlash_IsUcbRangeValid(uint8 ucbIndex,
                                         uint32 offset,
                                         uint32 length)
{
    uint32 start;
    uint32 end;
    uint32 size;

    if ((SotaFlash_IsUcbIndexValid(ucbIndex) == FALSE) || (length == 0U))
    {
        return FALSE;
    }

    start = IfxFlash_dFlashTableUcbLog[ucbIndex].start;
    end = IfxFlash_dFlashTableUcbLog[ucbIndex].end;
    size = end - start + 1U;

    if ((end < start) || (offset >= size) || (length > (size - offset)))
    {
        return FALSE;
    }

    return TRUE;
}

static boolean SotaFlash_IsRuntimeSwapUcb(uint8 ucbIndex)
{
    return ((ucbIndex == SOTA_FLASH_UCB_SWAP_ORIG_INDEX) ||
            (ucbIndex == SOTA_FLASH_UCB_SWAP_COPY_INDEX)) ? TRUE : FALSE;
}

#if defined(__TASKING__)
#pragma section code "cpu0_psram"
#endif

static SotaFlash_Result_t SOTA_FLASH_RAM_CODE SotaFlash_ProgramPage8(uint32 pageAddress,
                                                                     const uint8 *pageData,
                                                                     IfxFlash_FlashType flashType)
{
    uint32 wordL;
    uint32 wordU;
    uint16 safetyPassword;
    boolean interruptState;

    (void)memcpy(&wordL, &pageData[0], sizeof(uint32));
    (void)memcpy(&wordU, &pageData[sizeof(uint32)], sizeof(uint32));

    interruptState = IfxCpu_disableInterrupts();

    if (IfxFlash_enterPageMode(pageAddress) != 0U)
    {
        IfxCpu_restoreInterrupts(interruptState);
        return SOTA_FLASH_RESULT_HARDWARE_ERROR;
    }

    if (IfxFlash_waitUnbusy(0U, flashType) != 0U)
    {
        IfxCpu_restoreInterrupts(interruptState);
        return SOTA_FLASH_RESULT_FLASH_BUSY_TIMEOUT;
    }

    IfxFlash_loadPage2X32(pageAddress, wordL, wordU);

    safetyPassword = IfxScuWdt_getSafetyWatchdogPasswordInline();
    IfxScuWdt_clearSafetyEndinitInline(safetyPassword);
    IfxFlash_writePage(pageAddress);
    IfxScuWdt_setSafetyEndinitInline(safetyPassword);

    if (IfxFlash_waitUnbusy(0U, flashType) != 0U)
    {
        IfxCpu_restoreInterrupts(interruptState);
        return SOTA_FLASH_RESULT_FLASH_BUSY_TIMEOUT;
    }

    IfxCpu_restoreInterrupts(interruptState);

    if (MODULE_DMU.HF_ERRSR.U != 0U)
    {
        return SOTA_FLASH_RESULT_HARDWARE_ERROR;
    }

    return SOTA_FLASH_RESULT_OK;
}

static SotaFlash_Result_t SOTA_FLASH_RAM_CODE SotaFlash_ErasePflashSectorCommand(uint32 sectorAddress,
                                                                                 IfxFlash_FlashType flashType)
{
    uint16 safetyPassword;
    boolean interruptState;

    interruptState = IfxCpu_disableInterrupts();

    safetyPassword = IfxScuWdt_getSafetyWatchdogPasswordInline();
    IfxScuWdt_clearSafetyEndinitInline(safetyPassword);
    IfxFlash_eraseSector(sectorAddress);
    IfxScuWdt_setSafetyEndinitInline(safetyPassword);

    /* Command cycles completed. Enable interrupts during busy-wait to maintain CAN-FD responsiveness. */
    IfxCpu_restoreInterrupts(interruptState);

    if (IfxFlash_waitUnbusy(0U, flashType) != 0U)
    {
        return SOTA_FLASH_RESULT_FLASH_BUSY_TIMEOUT;
    }

    if (MODULE_DMU.HF_ERRSR.U != 0U)
    {
        return SOTA_FLASH_RESULT_HARDWARE_ERROR;
    }

    return SOTA_FLASH_RESULT_OK;
}

static SotaFlash_Result_t SOTA_FLASH_RAM_CODE SotaFlash_ProgramPflashPage32(uint32 pageAddress,
                                                                            const uint8 *pageData,
                                                                            IfxFlash_FlashType flashType)
{
    uint32 words[8];
    uint32 index;
    uint16 safetyPassword;
    boolean interruptState;

    (void)memcpy(words, pageData, SOTA_FLASH_PFLASH_PAGE_SIZE_BYTE);

    interruptState = IfxCpu_disableInterrupts();

    if (IfxFlash_enterPageMode(pageAddress) != 0U)
    {
        IfxCpu_restoreInterrupts(interruptState);
        return SOTA_FLASH_RESULT_HARDWARE_ERROR;
    }

    if (IfxFlash_waitUnbusy(0U, flashType) != 0U)
    {
        IfxCpu_restoreInterrupts(interruptState);
        return SOTA_FLASH_RESULT_FLASH_BUSY_TIMEOUT;
    }

    for (index = 0U; index < 8U; index += 2U)
    {
        IfxFlash_loadPage2X32(pageAddress, words[index], words[index + 1U]);
    }

    safetyPassword = IfxScuWdt_getSafetyWatchdogPasswordInline();
    IfxScuWdt_clearSafetyEndinitInline(safetyPassword);
    IfxFlash_writePage(pageAddress);
    IfxScuWdt_setSafetyEndinitInline(safetyPassword);

    /* Command cycles completed. Enable interrupts during busy-wait to maintain CAN-FD responsiveness. */
    IfxCpu_restoreInterrupts(interruptState);

    if (IfxFlash_waitUnbusy(0U, flashType) != 0U)
    {
        return SOTA_FLASH_RESULT_FLASH_BUSY_TIMEOUT;
    }

    if (MODULE_DMU.HF_ERRSR.U != 0U)
    {
        return SOTA_FLASH_RESULT_HARDWARE_ERROR;
    }

    return SOTA_FLASH_RESULT_OK;
}

#if defined(__TASKING__)
#pragma section code restore
#endif
