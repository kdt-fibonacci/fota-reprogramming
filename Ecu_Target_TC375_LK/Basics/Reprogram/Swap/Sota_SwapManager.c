#include "Sota_SwapManager.h"

#define SOTA_SWAP_MANAGER_INVALID_ENTRY_INDEX (0xFFFFU)

#if (SOTA_SWAPMANAGER_USE_SWAPDIAG == 1U)
#include "Sota_SwapDiag.h"
#endif

typedef struct
{
    SotaTc37x_PflashBank_t previousBank;
    uint16 lastSwapEntryIndex;
} SotaSwapManager_Context_t;

static SotaSwapManager_Context_t g_sotaSwapManager;

static boolean SotaSwapManager_IsKnownBank(SotaTc37x_PflashBank_t bank);
static SotaSwapManager_Result_t SotaSwapManager_AppendSwapEntry(SotaTc37x_PflashBank_t targetBank);

void SotaSwapManager_Init(void)
{
    g_sotaSwapManager.previousBank = SotaTc37x_GetCurrentActiveBank();
    g_sotaSwapManager.lastSwapEntryIndex = SOTA_SWAP_MANAGER_INVALID_ENTRY_INDEX;
}

SotaTc37x_PflashBank_t SotaSwapManager_GetCurrentBank(void)
{
    return SotaTc37x_GetCurrentActiveBank();
}

SotaTc37x_PflashBank_t SotaSwapManager_GetInactiveBank(void)
{
    return SotaTc37x_GetInactiveBank();
}

SotaSwapManager_Result_t SotaSwapManager_ArmSwapToBank(SotaTc37x_PflashBank_t targetBank)
{
    SotaTc37x_PflashBank_t currentBank = SotaTc37x_GetCurrentActiveBank();

    if ((SotaSwapManager_IsKnownBank(currentBank) == FALSE) ||
        (SotaSwapManager_IsKnownBank(targetBank) == FALSE))
    {
        return SOTA_SWAP_MANAGER_RESULT_INVALID_PARAM;
    }

    if (targetBank == currentBank)
    {
        return SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
    }

    if (targetBank != SotaTc37x_GetInactiveBank())
    {
        return SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
    }

    g_sotaSwapManager.previousBank = currentBank;

    /*
     * Runtime activation only appends the next UCB_SWAP entry. The SSW applies
     * the swap on the next system reset; an application reset is not enough.
     * The previous bank must not be erased until the new image is COMMITTED.
     */
    return SotaSwapManager_AppendSwapEntry(targetBank);
}

SotaSwapManager_Result_t SotaSwapManager_ArmRollbackToPreviousBank(void)
{
    SotaTc37x_PflashBank_t currentBank = SotaTc37x_GetCurrentActiveBank();
    SotaTc37x_PflashBank_t rollbackBank = g_sotaSwapManager.previousBank;

    if (SotaSwapManager_IsKnownBank(currentBank) == FALSE)
    {
        return SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
    }

    /*
     * After SSW applies a swap and performs a system reset, RAM state is gone.
     * In that case the previous image bank is the current inactive bank.
     */
    if ((SotaSwapManager_IsKnownBank(rollbackBank) == FALSE) ||
        (rollbackBank == currentBank))
    {
        rollbackBank = SotaTc37x_GetInactiveBank();
    }

    if ((SotaSwapManager_IsKnownBank(rollbackBank) == FALSE) ||
        (rollbackBank == currentBank))
    {
        return SOTA_SWAP_MANAGER_RESULT_INVALID_STATE;
    }

    /*
     * Rollback is also expressed as an appended UCB_SWAP entry back to the
     * previous bank. SSW consumes it only after a system reset.
     */
    return SotaSwapManager_AppendSwapEntry(rollbackBank);
}

uint16 SotaSwapManager_GetLastSwapEntryIndex(void)
{
    return g_sotaSwapManager.lastSwapEntryIndex;
}

#if (SOTA_ENABLE_FACTORY_PROVISIONING == 1U)
SotaSwapManager_Result_t SotaSwapManager_FactoryEnableOtp(void)
{
    /*
     * UCB_OTP provisioning is intentionally excluded from the production
     * runtime path. Enable SOTA_ENABLE_FACTORY_PROVISIONING only in controlled
     * factory/debug builds.
     */
#if (SOTA_SWAPMANAGER_USE_SWAPDIAG == 1U)
    return SotaSwapDiag_FactoryEnableOtp();
#else
    return SOTA_SWAP_MANAGER_RESULT_NOT_CONFIGURED;
#endif
}

SotaSwapManager_Result_t SotaSwapManager_FactoryEraseAndReinitSwap(void)
{
    /*
     * UCB_SWAP erase/reinitialization is factory/debug-only. Runtime update
     * code must append entries instead of resetting the UCB_SWAP area.
     */
#if (SOTA_SWAPMANAGER_USE_SWAPDIAG == 1U)
    return SotaSwapDiag_FactoryEraseAndReinitSwap();
#else
    return SOTA_SWAP_MANAGER_RESULT_NOT_CONFIGURED;
#endif
}
#endif

static boolean SotaSwapManager_IsKnownBank(SotaTc37x_PflashBank_t bank)
{
    return ((bank == SOTA_TC37X_PFLASH_BANK_PF0) ||
            (bank == SOTA_TC37X_PFLASH_BANK_PF1)) ? TRUE : FALSE;
}

static SotaSwapManager_Result_t SotaSwapManager_AppendSwapEntry(SotaTc37x_PflashBank_t targetBank)
{
#if (SOTA_SWAPMANAGER_USE_SWAPDIAG == 1U)
    uint16 entryIndex = SOTA_SWAP_MANAGER_INVALID_ENTRY_INDEX;
    SotaSwapManager_Result_t result;

    result = SotaSwapDiag_AppendNextSwapEntry((uint8)targetBank, &entryIndex);
    if (result == SOTA_SWAP_MANAGER_RESULT_OK)
    {
        g_sotaSwapManager.lastSwapEntryIndex = entryIndex;
    }

    return result;
#else
    (void)targetBank;
    return SOTA_SWAP_MANAGER_RESULT_NOT_CONFIGURED;
#endif
}
