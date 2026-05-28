/*
 * Sota_Tc37x_Config.c
 *
 * TC37x/TC375 SOTA flash layout helper functions.
 */

/******************************************************************************/
/*----------------------------------Includes----------------------------------*/
/******************************************************************************/
#include "Sota_Tc37x_Config.h"
#include "Sota_SwapDiag.h"

/******************************************************************************/
/*-------------------------Function Implementations---------------------------*/
/******************************************************************************/
boolean SotaTc37x_IsPf0Address(uint32 addr)
{
    return (boolean)((addr >= TC37X_PF0_NC_START) && (addr <= TC37X_PF0_NC_END));
}

boolean SotaTc37x_IsPf1Address(uint32 addr)
{
    return (boolean)((addr >= TC37X_PF1_NC_START) && (addr <= TC37X_PF1_NC_END));
}

SotaBank SotaTc37x_GetBankByAddress(uint32 addr)
{
    SotaBank bank = SOTA_BANK_INVALID;

    if (SotaTc37x_IsPf0Address(addr) != FALSE)
    {
        bank = SOTA_BANK_PF0;
    }
    else if (SotaTc37x_IsPf1Address(addr) != FALSE)
    {
        bank = SOTA_BANK_PF1;
    }
    else
    {
        bank = SOTA_BANK_INVALID;
    }

    return bank;
}

boolean SotaTc37x_GetPFlashType(uint32 addr, IfxFlash_FlashType *type)
{
    boolean ret = FALSE;

    if (type == NULL_PTR)
    {
        return FALSE;
    }

    if (SotaTc37x_IsPf0Address(addr) != FALSE)
    {
        *type = IfxFlash_FlashType_P0;
        ret = TRUE;
    }
    else if (SotaTc37x_IsPf1Address(addr) != FALSE)
    {
        *type = IfxFlash_FlashType_P1;
        ret = TRUE;
    }
    else
    {
        ret = FALSE;
    }

    return ret;
}

boolean SotaTc37x_IsValidPflashRange(uint32 addr, uint32 len)
{
    uint32 endAddr;

    if (len == 0u)
    {
        return FALSE;
    }

    if (len > TC37X_PFLASH_BANK_SIZE)
    {
        return FALSE;
    }

    if (addr >= TC37X_PFLASH_NC_INVALID_START)
    {
        return FALSE;
    }

    if (addr > (0xFFFFFFFFu - (len - 1u)))
    {
        return FALSE;
    }

    endAddr = addr + len - 1u;

    if (endAddr >= TC37X_PFLASH_NC_INVALID_START)
    {
        return FALSE;
    }

    if (SotaTc37x_IsPf0Address(addr) != FALSE)
    {
        return (boolean)(endAddr <= TC37X_PF0_NC_END);
    }

    if (SotaTc37x_IsPf1Address(addr) != FALSE)
    {
        return (boolean)(endAddr <= TC37X_PF1_NC_END);
    }

    return FALSE;
}

uint32 SotaTc37x_GetBankStart(SotaBank bank)
{
    uint32 startAddr = SOTA_INVALID_ADDRESS;

    switch (bank)
    {
    case SOTA_BANK_PF0:
        startAddr = TC37X_PF0_NC_START;
        break;

    case SOTA_BANK_PF1:
        startAddr = TC37X_PF1_NC_START;
        break;

    default:
        break;
    }

    return startAddr;
}

uint32 SotaTc37x_GetBankEnd(SotaBank bank)
{
    uint32 endAddr = SOTA_INVALID_ADDRESS;

    switch (bank)
    {
    case SOTA_BANK_PF0:
        endAddr = TC37X_PF0_NC_END;
        break;

    case SOTA_BANK_PF1:
        endAddr = TC37X_PF1_NC_END;
        break;

    default:
        break;
    }

    return endAddr;
}

SotaBank SotaTc37x_GetActiveBank(void)
{
    SotaBank activeBank = SOTA_BANK_INVALID;
    uint8 currentMode = SotaSwap_GetCurrentMode();

    /* These helpers return physical PF0/PF1 banks in the standard address map.
     * Flash erase/program must use these physical addresses, not logical swapped
     * address assumptions. */
    if (currentMode == SOTA_SWAP_STANDARD)
    {
        activeBank = SOTA_BANK_PF0;
    }
    else if (currentMode == SOTA_SWAP_ALTERNATE)
    {
        activeBank = SOTA_BANK_PF1;
    }
    else
    {
        activeBank = SOTA_BANK_INVALID;
    }

    return activeBank;
}

SotaBank SotaTc37x_GetInactiveBank(void)
{
    SotaBank inactiveBank = SOTA_BANK_INVALID;
    uint8 currentMode = SotaSwap_GetCurrentMode();

    if (currentMode == SOTA_SWAP_STANDARD)
    {
        inactiveBank = SOTA_BANK_PF1;
    }
    else if (currentMode == SOTA_SWAP_ALTERNATE)
    {
        inactiveBank = SOTA_BANK_PF0;
    }
    else
    {
        inactiveBank = SOTA_BANK_INVALID;
    }

    return inactiveBank;
}

uint32 SotaTc37x_GetInactiveBankStart(void)
{
    return SotaTc37x_GetBankStart(SotaTc37x_GetInactiveBank());
}

uint32 SotaTc37x_GetInactiveBankEnd(void)
{
    return SotaTc37x_GetBankEnd(SotaTc37x_GetInactiveBank());
}
