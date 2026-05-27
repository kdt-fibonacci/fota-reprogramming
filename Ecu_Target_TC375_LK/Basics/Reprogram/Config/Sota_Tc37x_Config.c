#include "Sota_Tc37x_Config.h"

#include "IfxScu_reg.h"

#define SOTA_TC37X_PFLASH_CACHED_ALIAS_START (0x80000000U)
#define SOTA_TC37X_PFLASH_CACHED_ALIAS_END   \
    (SOTA_TC37X_PFLASH_CACHED_ALIAS_START +  \
     (2U * SOTA_TC37X_PFLASH_BANK_SIZE_BYTE) - 1U)
#define SOTA_TC37X_PFLASH_SYSTEM_ALIAS_START SOTA_TC37X_PFLASH_BANK0_SYSTEM_START
#define SOTA_TC37X_PFLASH_SYSTEM_ALIAS_END   \
    (SOTA_TC37X_PFLASH_SYSTEM_ALIAS_START +  \
     (2U * SOTA_TC37X_PFLASH_BANK_SIZE_BYTE) - 1U)
#define SOTA_TC37X_CACHED_TO_SYSTEM_OFFSET   (0x20000000U)
#define SOTA_TC37X_INVALID_ADDRESS           (0xFFFFFFFFU)
#define SOTA_TC37X_SWAPCTRL_PF0_ACTIVE       (1U)
#define SOTA_TC37X_SWAPCTRL_PF1_ACTIVE       (2U)

static boolean SotaTc37x_IsKnownBank(SotaTc37x_PflashBank_t bank);
static boolean SotaTc37x_IsRangeInsideBank(uint32 address,
                                           uint32 length,
                                           SotaTc37x_PflashBank_t bank);

uint32 SotaTc37x_NormalizePflashAddress(uint32 address)
{
    uint32 normalizedAddress;

    if ((address >= SOTA_TC37X_PFLASH_CACHED_ALIAS_START) &&
        (address <= SOTA_TC37X_PFLASH_CACHED_ALIAS_END))
    {
        normalizedAddress = address + SOTA_TC37X_CACHED_TO_SYSTEM_OFFSET;
    }
    else if ((address >= SOTA_TC37X_PFLASH_SYSTEM_ALIAS_START) &&
             (address <= SOTA_TC37X_PFLASH_SYSTEM_ALIAS_END))
    {
        normalizedAddress = address;
    }
    else
    {
        normalizedAddress = SOTA_TC37X_INVALID_ADDRESS;
    }

    return normalizedAddress;
}

SotaTc37x_PflashBank_t SotaTc37x_GetBankByAddress(uint32 address)
{
    SotaTc37x_PflashBank_t bank = SOTA_TC37X_PFLASH_BANK_UNKNOWN;
    uint32 normalizedAddress = SotaTc37x_NormalizePflashAddress(address);

    if ((normalizedAddress >= SOTA_TC37X_PFLASH_BANK0_SYSTEM_START) &&
        (normalizedAddress <=
         (SOTA_TC37X_PFLASH_BANK0_SYSTEM_START +
          SOTA_TC37X_PFLASH_BANK_SIZE_BYTE - 1U)))
    {
        bank = SOTA_TC37X_PFLASH_BANK_PF0;
    }
    else if ((normalizedAddress >= SOTA_TC37X_PFLASH_BANK1_SYSTEM_START) &&
             (normalizedAddress <=
              (SOTA_TC37X_PFLASH_BANK1_SYSTEM_START +
               SOTA_TC37X_PFLASH_BANK_SIZE_BYTE - 1U)))
    {
        bank = SOTA_TC37X_PFLASH_BANK_PF1;
    }
    else
    {
        bank = SOTA_TC37X_PFLASH_BANK_UNKNOWN;
    }

    return bank;
}

SotaTc37x_PflashBank_t SotaTc37x_GetCurrentActiveBank(void)
{
    SotaTc37x_PflashBank_t bank = SOTA_TC37X_PFLASH_BANK_UNKNOWN;

    if ((uint8)SOTA_TC37X_ACTIVE_BANK_OVERRIDE !=
        (uint8)SOTA_TC37X_PFLASH_BANK_UNKNOWN)
    {
        bank = (SotaTc37x_PflashBank_t)SOTA_TC37X_ACTIVE_BANK_OVERRIDE;
    }
#if (SOTA_TC37X_ENABLE_SWAPCTRL_ACTIVE_BANK_DETECT == 1U)
    else if (SCU_SWAPCTRL.B.ADDRCFG == SOTA_TC37X_SWAPCTRL_PF0_ACTIVE)
    {
        bank = SOTA_TC37X_PFLASH_BANK_PF0;
    }
    else if (SCU_SWAPCTRL.B.ADDRCFG == SOTA_TC37X_SWAPCTRL_PF1_ACTIVE)
    {
        bank = SOTA_TC37X_PFLASH_BANK_PF1;
    }
#endif
#if (SOTA_TC37X_ENABLE_ADDRESS_BASED_ACTIVE_BANK_DETECT == 1U)
    else
    {
        bank = SotaTc37x_GetBankByAddress((uint32)&SotaTc37x_GetCurrentActiveBank);
    }
#endif

    return bank;
}

SotaTc37x_PflashBank_t SotaTc37x_GetInactiveBank(void)
{
    SotaTc37x_PflashBank_t inactiveBank;

    switch (SotaTc37x_GetCurrentActiveBank())
    {
    case SOTA_TC37X_PFLASH_BANK_PF0:
        inactiveBank = SOTA_TC37X_PFLASH_BANK_PF1;
        break;

    case SOTA_TC37X_PFLASH_BANK_PF1:
        inactiveBank = SOTA_TC37X_PFLASH_BANK_PF0;
        break;

    default:
        inactiveBank = SOTA_TC37X_PFLASH_BANK_UNKNOWN;
        break;
    }

    return inactiveBank;
}

uint32 SotaTc37x_GetBankStart(SotaTc37x_PflashBank_t bank)
{
    uint32 start;

    switch (bank)
    {
    case SOTA_TC37X_PFLASH_BANK_PF0:
        start = SOTA_TC37X_PFLASH_BANK0_SYSTEM_START;
        break;

    case SOTA_TC37X_PFLASH_BANK_PF1:
        start = SOTA_TC37X_PFLASH_BANK1_SYSTEM_START;
        break;

    default:
        start = 0U;
        break;
    }

    return start;
}

uint32 SotaTc37x_GetBankEnd(SotaTc37x_PflashBank_t bank)
{
    uint32 start = SotaTc37x_GetBankStart(bank);

    return (start == 0U) ? 0U : (start + SOTA_TC37X_PFLASH_BANK_SIZE_BYTE - 1U);
}

uint32 SotaTc37x_GetBankSize(SotaTc37x_PflashBank_t bank)
{
    return (SotaTc37x_IsKnownBank(bank) == TRUE) ?
           SOTA_TC37X_PFLASH_BANK_SIZE_BYTE : 0U;
}

uint32 SotaTc37x_GetInactiveBankStart(void)
{
    return SotaTc37x_GetBankStart(SotaTc37x_GetInactiveBank());
}

uint32 SotaTc37x_GetInactiveBankSize(void)
{
    return SotaTc37x_GetBankSize(SotaTc37x_GetInactiveBank());
}

boolean SotaTc37x_IsRangeInsideInactiveBank(uint32 address, uint32 length)
{
    return (SotaTc37x_ValidateInactiveBankRange(address, length) ==
            SOTA_TC37X_RANGE_STATUS_OK) ? TRUE : FALSE;
}

SotaTc37x_RangeStatus_t SotaTc37x_ValidateInactiveBankRange(uint32 address,
                                                            uint32 length)
{
    uint32 normalizedAddress;
    SotaTc37x_PflashBank_t activeBank;
    SotaTc37x_PflashBank_t targetBank;

    if (length == 0U)
    {
        return SOTA_TC37X_RANGE_STATUS_INVALID_LENGTH;
    }

    normalizedAddress = SotaTc37x_NormalizePflashAddress(address);
    if (normalizedAddress == SOTA_TC37X_INVALID_ADDRESS)
    {
        return SOTA_TC37X_RANGE_STATUS_INVALID_ADDRESS;
    }

    targetBank = SotaTc37x_GetBankByAddress(normalizedAddress);
    if (targetBank == SOTA_TC37X_PFLASH_BANK_UNKNOWN)
    {
        return SOTA_TC37X_RANGE_STATUS_INVALID_ADDRESS;
    }

    activeBank = SotaTc37x_GetCurrentActiveBank();
    if (activeBank == SOTA_TC37X_PFLASH_BANK_UNKNOWN)
    {
        return SOTA_TC37X_RANGE_STATUS_INVALID_ADDRESS;
    }

    if (targetBank == activeBank)
    {
        return SOTA_TC37X_RANGE_STATUS_ACTIVE_BANK_ACCESS;
    }

    if (targetBank != SotaTc37x_GetInactiveBank())
    {
        return SOTA_TC37X_RANGE_STATUS_INVALID_ADDRESS;
    }

    if (SotaTc37x_IsRangeInsideBank(normalizedAddress,
                                    length,
                                    targetBank) == FALSE)
    {
        return SOTA_TC37X_RANGE_STATUS_INVALID_LENGTH;
    }

    return SOTA_TC37X_RANGE_STATUS_OK;
}

static boolean SotaTc37x_IsKnownBank(SotaTc37x_PflashBank_t bank)
{
    return ((bank == SOTA_TC37X_PFLASH_BANK_PF0) ||
            (bank == SOTA_TC37X_PFLASH_BANK_PF1)) ? TRUE : FALSE;
}

static boolean SotaTc37x_IsRangeInsideBank(uint32 address,
                                           uint32 length,
                                           SotaTc37x_PflashBank_t bank)
{
    uint32 normalizedAddress;
    uint32 rangeEnd;
    uint32 bankStart;
    uint32 bankEnd;

    if ((SotaTc37x_IsKnownBank(bank) == FALSE) || (length == 0U))
    {
        return FALSE;
    }

    normalizedAddress = SotaTc37x_NormalizePflashAddress(address);
    if (normalizedAddress == SOTA_TC37X_INVALID_ADDRESS)
    {
        return FALSE;
    }

    rangeEnd = normalizedAddress + length - 1U;
    if (rangeEnd < normalizedAddress)
    {
        return FALSE;
    }

    bankStart = SotaTc37x_GetBankStart(bank);
    bankEnd = SotaTc37x_GetBankEnd(bank);

    return ((normalizedAddress >= bankStart) && (rangeEnd <= bankEnd)) ?
           TRUE : FALSE;
}
