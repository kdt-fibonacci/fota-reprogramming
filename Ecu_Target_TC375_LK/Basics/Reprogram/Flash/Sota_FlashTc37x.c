/**
 * \file Sota_FlashTc37x.c
 * \brief TC37x-safe SOTA flash helpers.
 *
 * \version iLLD_Demos_1_0_1_10_0
 * \copyright Copyright (c) 2014 Infineon Technologies AG. All rights reserved.
 *
 *
 *                                 IMPORTANT NOTICE
 *
 *
 * Infineon Technologies AG (Infineon) is supplying this file for use
 * exclusively with Infineon's microcontroller products. This file can be freely
 * distributed within development tools that are supporting such microcontroller
 * products.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 * OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 * INFINEON SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 */

/******************************************************************************/
/*----------------------------------Includes----------------------------------*/
/******************************************************************************/

#include <Cpu/Std/IfxCpu.h>
#include <string.h>
#include <IfxScuWdt.h>
#include <Sota/Flash/Sota_FlashTc37x.h>
#include <Sota/Sota_Tc37x_Config.h>

/******************************************************************************/
/*-----------------------------------Macros-----------------------------------*/
/******************************************************************************/

/******************************************************************************/
/*--------------------------------Enumerations--------------------------------*/
/******************************************************************************/

/******************************************************************************/
/*-----------------------------Data Structures--------------------------------*/
/******************************************************************************/

/******************************************************************************/
/*------------------------------Global variables------------------------------*/
/******************************************************************************/

/******************************************************************************/
/*-------------------------Function Prototypes--------------------------------*/
/******************************************************************************/
/******************************************************************************/
/*------------------------Private Variables/Constants-------------------------*/
/******************************************************************************/

/******************************************************************************/
/*-------------------------Function Implementations---------------------------*/
/******************************************************************************/

typedef void  (*SotaFlashPsprEraseSectors)(uint32 sectorAddr, uint32 numSector);
typedef uint8 (*SotaFlashPsprWaitUnbusy)(uint32 flash, IfxFlash_FlashType flashType);
typedef uint8 (*SotaFlashPsprEnterPageMode)(uint32 pageAddr);
typedef void  (*SotaFlashPsprLoadPage2X32)(uint32 pageAddr, uint32 wordL, uint32 wordU);
typedef void  (*SotaFlashPsprWritePage)(uint32 pageAddr);
typedef uint8 (*SotaFlashPsprErasePflashSector)(uint32 sectorAddr, IfxFlash_FlashType flashType);
typedef uint8 (*SotaFlashPsprProgramPflashPage32)(uint32 pageAddr, const uint32 *words, IfxFlash_FlashType flashType);
typedef uint8 (*SotaFlashPsprProgramDflashPage8)(uint32 pageAddr, const uint32 *words, IfxFlash_FlashType flashType);

typedef struct
{
    SotaFlashPsprEraseSectors eraseSectors;
    SotaFlashPsprWaitUnbusy waitUnbusy;
    SotaFlashPsprEnterPageMode enterPageMode;
    SotaFlashPsprLoadPage2X32 loadPage2X32;
    SotaFlashPsprWritePage writePage;
    SotaFlashPsprErasePflashSector erasePflashSector;
    SotaFlashPsprProgramPflashPage32 programPflashPage32;
    SotaFlashPsprProgramDflashPage8 programDflashPage8;
} SotaFlashPsprCommands;

static SotaFlashPsprCommands sotaFlashPsprCommands;
static boolean sotaFlashPsprCommandsReady = FALSE;

/*
 * TC375 flash programming must execute command sequences from RAM/PSPR. The
 * TASKING linker script maps .text.cpu0_psram* to code_psram0 with
 * run_addr=mem:psram0 and copy enabled. The previous FLSLOADERRAMCODE section
 * was left in PFLASH by this project map, so only these command routines are
 * selected into CPU0 PSPR.
 */
#if defined(__HIGHTEC__)
#pragma section
#pragma section ".cpu0_psram.sota_flash" awx
#endif

#if defined(__TASKING__)
#pragma section code "cpu0_psram"
#endif

static void SotaFlash_PsprEraseSectors(uint32 sectorAddr, uint32 numSector)
{
    IfxFlash_eraseMultipleSectors(sectorAddr, numSector);
}

static uint8 SotaFlash_PsprWaitUnbusy(uint32 flash, IfxFlash_FlashType flashType)
{
    return IfxFlash_waitUnbusy(flash, flashType);
}

static uint8 SotaFlash_PsprEnterPageMode(uint32 pageAddr)
{
    return IfxFlash_enterPageMode(pageAddr);
}

static void SotaFlash_PsprLoadPage2X32(uint32 pageAddr, uint32 wordL, uint32 wordU)
{
    IfxFlash_loadPage2X32(pageAddr, wordL, wordU);
}

static void SotaFlash_PsprWritePage(uint32 pageAddr)
{
    IfxFlash_writePage(pageAddr);
}

static uint8 SotaFlash_PsprErasePflashSector(uint32 sectorAddr, IfxFlash_FlashType flashType)
{
    boolean interruptsEnabled;
    uint16 safetyPassword;
    uint8 ret;

    interruptsEnabled = IfxCpu_disableInterrupts();
    safetyPassword = IfxScuWdt_getSafetyWatchdogPasswordInline();

    IfxScuWdt_clearSafetyEndinitInline(safetyPassword);
    SotaFlash_PsprEraseSectors(sectorAddr, 1u);
    IfxScuWdt_setSafetyEndinitInline(safetyPassword);

    ret = SotaFlash_PsprWaitUnbusy(0u, flashType);
    IfxCpu_restoreInterrupts(interruptsEnabled);

    if (ret != FLASH_RESULT_OK)
    {
        return ret;
    }

    if (MODULE_DMU.HF_ERRSR.U != 0u)
    {
        return FLASH_RESULT_DMU_ERROR;
    }

    return FLASH_RESULT_OK;
}

static uint8 SotaFlash_PsprProgramPflashPage32(uint32 pageAddr, const uint32 *words, IfxFlash_FlashType flashType)
{
    boolean interruptsEnabled;
    uint16 safetyPassword;
    uint8 ret;

    interruptsEnabled = IfxCpu_disableInterrupts();

    if (SotaFlash_PsprEnterPageMode(pageAddr) != 0u)
    {
        IfxCpu_restoreInterrupts(interruptsEnabled);
        return FLASH_RESULT_ENTER_PAGE_MODE_FAILED;
    }

    ret = SotaFlash_PsprWaitUnbusy(0u, flashType);
    if (ret != FLASH_RESULT_OK)
    {
        IfxCpu_restoreInterrupts(interruptsEnabled);
        return ret;
    }

    SotaFlash_PsprLoadPage2X32(pageAddr, words[0], words[1]);
    SotaFlash_PsprLoadPage2X32(pageAddr, words[2], words[3]);
    SotaFlash_PsprLoadPage2X32(pageAddr, words[4], words[5]);
    SotaFlash_PsprLoadPage2X32(pageAddr, words[6], words[7]);

    safetyPassword = IfxScuWdt_getSafetyWatchdogPasswordInline();
    IfxScuWdt_clearSafetyEndinitInline(safetyPassword);
    SotaFlash_PsprWritePage(pageAddr);
    IfxScuWdt_setSafetyEndinitInline(safetyPassword);

    ret = SotaFlash_PsprWaitUnbusy(0u, flashType);
    IfxCpu_restoreInterrupts(interruptsEnabled);

    if (ret != FLASH_RESULT_OK)
    {
        return ret;
    }

    if (MODULE_DMU.HF_ERRSR.U != 0u)
    {
        return FLASH_RESULT_DMU_ERROR;
    }

    return FLASH_RESULT_OK;
}

static uint8 SotaFlash_PsprProgramDflashPage8(uint32 pageAddr, const uint32 *words, IfxFlash_FlashType flashType)
{
    boolean interruptsEnabled;
    uint16 safetyPassword;
    uint8 ret;

    interruptsEnabled = IfxCpu_disableInterrupts();

    if (SotaFlash_PsprEnterPageMode(pageAddr) != 0u)
    {
        IfxCpu_restoreInterrupts(interruptsEnabled);
        return FLASH_RESULT_ENTER_PAGE_MODE_FAILED;
    }

    ret = SotaFlash_PsprWaitUnbusy(0u, flashType);
    if (ret != FLASH_RESULT_OK)
    {
        IfxCpu_restoreInterrupts(interruptsEnabled);
        return ret;
    }

    SotaFlash_PsprLoadPage2X32(pageAddr, words[0], words[1]);

    safetyPassword = IfxScuWdt_getSafetyWatchdogPasswordInline();
    IfxScuWdt_clearSafetyEndinitInline(safetyPassword);
    SotaFlash_PsprWritePage(pageAddr);
    IfxScuWdt_setSafetyEndinitInline(safetyPassword);

    ret = SotaFlash_PsprWaitUnbusy(0u, flashType);
    IfxCpu_restoreInterrupts(interruptsEnabled);

    if (ret != FLASH_RESULT_OK)
    {
        return ret;
    }

    if (MODULE_DMU.HF_ERRSR.U != 0u)
    {
        return FLASH_RESULT_DMU_ERROR;
    }

    return FLASH_RESULT_OK;
}

#if defined(__HIGHTEC__)
#pragma section
#endif

#if defined(__TASKING__)
#pragma section code restore
#endif

static boolean Flash_IsUcbWriteEnabled(uint8 ucb_no)
{
#if (SOTA_ENABLE_UCB_WRITES != 0u)
	if ((ucb_no == UCB_SWAP_ORIG_NO) || (ucb_no == UCB_SWAP_COPY_NO))
	{
		return (boolean)(SOTA_ENABLE_UCB_SWAP_WRITES != 0u);
	}

	if ((ucb_no == UCB_OTP_ORIG_NO) || (ucb_no == UCB_OTP_COPY_NO))
	{
		return (boolean)(SOTA_ENABLE_UCB_OTP_WRITES != 0u);
	}

	return FALSE;
#else
	(void)ucb_no;
	return FALSE;
#endif
}

static uint8 Flash_GetTc37xPflashRangeError(uint32 addr, uint32 len)
{
	uint32 endAddr;

	if ((len == 0u) || (addr > (0xFFFFFFFFu - (len - 1u))))
	{
		return FLASH_RESULT_INVALID_PFLASH_RANGE;
	}

	endAddr = addr + len - 1u;

	if ((SotaTc37x_GetBankByAddress(addr) != SOTA_BANK_INVALID) &&
	    (SotaTc37x_GetBankByAddress(endAddr) != SotaTc37x_GetBankByAddress(addr)))
	{
		return FLASH_RESULT_BANK_BOUNDARY;
	}

	return FLASH_RESULT_INVALID_PFLASH_RANGE;
}

static boolean Flash_GetRangeEnd(uint32 addr, uint32 len, uint32 *endAddr)
{
	if ((len == 0u) || (endAddr == NULL_PTR))
	{
		return FALSE;
	}

	if (addr > (0xFFFFFFFFu - (len - 1u)))
	{
		return FALSE;
	}

	*endAddr = addr + len - 1u;
	return TRUE;
}

static boolean Flash_IsRangeInTable(uint32 addr, uint32 len, const IfxFlash_flashSector *table, uint32 tableCount)
{
	uint32 index;
	uint32 endAddr;

	if (Flash_GetRangeEnd(addr, len, &endAddr) == FALSE)
	{
		return FALSE;
	}

	for (index = 0u; index < tableCount; index++)
	{
		if ((table[index].start == 0u) && (table[index].end == 0u))
		{
			continue;
		}

		if ((addr >= table[index].start) && (endAddr <= table[index].end))
		{
			return TRUE;
		}
	}

	return FALSE;
}

static boolean Flash_IsTc37xUcbRange(uint32 addr, uint32 len)
{
	return Flash_IsRangeInTable(addr, len, IfxFlash_dFlashTableUcbLog, IFXFLASH_DFLASH_NUM_UCB_LOG_SECTORS);
}

static boolean Flash_IsTc37xDflashRange(uint32 addr, uint32 len)
{
	if (Flash_IsRangeInTable(addr, len, IfxFlash_dFlashTableEepLog, IFXFLASH_DFLASH_NUM_LOG_SECTORS) != FALSE)
	{
		return TRUE;
	}

	if (Flash_IsRangeInTable(addr, len, IfxFlash_dFlashTableCfsLog, IFXFLASH_DFLASH_NUM_CFS_LOG_SECTORS) != FALSE)
	{
		return TRUE;
	}

	if (Flash_IsRangeInTable(addr, len, IfxFlash_dFlashTableHsmLog, IFXFLASH_DFLASH_NUM_HSM_LOG_SECTORS) != FALSE)
	{
		return TRUE;
	}

	return FALSE;
}

static uint8 Flash_ValidateTc37xProgramFlashRequest(uint32 addr, uint32 len, IfxFlash_FlashType flashType)
{
	IfxFlash_FlashType addrFlashType;
	SotaBank addrBank;
	SotaBank activeBank;

	addrBank = SotaTc37x_GetBankByAddress(addr);

	if (addrBank != SOTA_BANK_INVALID)
	{
		if ((flashType != IfxFlash_FlashType_P0) && (flashType != IfxFlash_FlashType_P1))
		{
			return FLASH_RESULT_UNSUPPORTED_FLASH_TYPE;
		}

		if (SotaTc37x_IsValidPflashRange(addr, len) == FALSE)
		{
			return Flash_GetTc37xPflashRangeError(addr, len);
		}

		if (SotaTc37x_GetPFlashType(addr, &addrFlashType) == FALSE)
		{
			return FLASH_RESULT_INVALID_PFLASH_RANGE;
		}

		if (addrFlashType != flashType)
		{
			return FLASH_RESULT_FLASH_TYPE_MISMATCH;
		}

		activeBank = SotaTc37x_GetActiveBank();

		if (activeBank == SOTA_BANK_INVALID)
		{
			return FLASH_RESULT_ACTIVE_BANK_UNKNOWN;
		}

		if (addrBank == activeBank)
		{
			return FLASH_RESULT_ACTIVE_BANK;
		}

		return FLASH_RESULT_OK;
	}

	if ((flashType == IfxFlash_FlashType_P0) || (flashType == IfxFlash_FlashType_P1))
	{
		return FLASH_RESULT_INVALID_PFLASH_RANGE;
	}

	if ((flashType == IfxFlash_FlashType_D0) || (flashType == IfxFlash_FlashType_D1))
	{
		if (Flash_IsTc37xUcbRange(addr, len) != FALSE)
		{
#if (SOTA_ENABLE_UCB_WRITES != 0u)
			return FLASH_RESULT_OK;
#else
			return FLASH_RESULT_UCB_WRITE_DISABLED;
#endif
		}

		if (Flash_IsTc37xDflashRange(addr, len) == FALSE)
		{
			return FLASH_RESULT_INVALID_DFLASH_RANGE;
		}

		return FLASH_RESULT_OK;
	}

	return FLASH_RESULT_UNSUPPORTED_FLASH_TYPE;
}

void SotaFlash_CopyPflashRoutinesToPspr(void)
{
    if (sotaFlashPsprCommandsReady == FALSE)
    {
        sotaFlashPsprCommands.eraseSectors = SotaFlash_PsprEraseSectors;
        sotaFlashPsprCommands.waitUnbusy = SotaFlash_PsprWaitUnbusy;
        sotaFlashPsprCommands.enterPageMode = SotaFlash_PsprEnterPageMode;
        sotaFlashPsprCommands.loadPage2X32 = SotaFlash_PsprLoadPage2X32;
        sotaFlashPsprCommands.writePage = SotaFlash_PsprWritePage;
        sotaFlashPsprCommands.erasePflashSector = SotaFlash_PsprErasePflashSector;
        sotaFlashPsprCommands.programPflashPage32 = SotaFlash_PsprProgramPflashPage32;
        sotaFlashPsprCommands.programDflashPage8 = SotaFlash_PsprProgramDflashPage8;
        sotaFlashPsprCommandsReady = TRUE;
    }
}

uint8 SotaFlash_EraseSector(uint32 flash, uint32 sector_addr, IfxFlash_FlashType flashType)
{
    IfxFlash_FlashType resolvedFlashType;
    uint8 ret;

    (void)flash;
    (void)flashType;

    ret = (uint8)SotaFlash_ValidatePflashWrite(sector_addr, TC37X_PFLASH_SECTOR_SIZE);
    if (ret != FLASH_RESULT_OK)
    {
    	return ret;
    }

    if (SotaTc37x_GetPFlashType(sector_addr, &resolvedFlashType) == FALSE)
    {
        return FLASH_RESULT_INVALID_PFLASH_RANGE;
    }

    SotaFlash_CopyPflashRoutinesToPspr();
    return sotaFlashPsprCommands.erasePflashSector(sector_addr, resolvedFlashType);
}

/** \brief PFlashProgram
 *
 * This function will program the P-Flash Sector
 * Note: This function shouldn't be executed from Flash it is trying to program.
 * Recommended to execute the routine from PSPR memory
 */
uint8 SotaFlash_ProgramPage256(uint32 flash, uint32 page_addr, uint8 *pData, IfxFlash_FlashType flashType)
{
    uint32 offset;
    uint8 ret;

    (void)flashType;

    if (pData == NULL_PTR)
    {
        return FLASH_RESULT_INVALID_PFLASH_RANGE;
    }

    ret = (uint8)SotaFlash_ValidatePflashWrite(page_addr, 0x100u);
    if (ret != FLASH_RESULT_OK)
    {
    	return ret;
    }

    for (offset = 0u; offset < 0x100u; offset += 32u)
    {
        ret = SotaFlash_ProgramPage32(flash, page_addr + offset, pData + offset, flashType);
        if (ret != FLASH_RESULT_OK)
        {
            return ret;
        }
    }

	return ret;
}

uint8 SotaFlash_ProgramPage32(uint32 flash, uint32 page_addr, uint8 *pData, IfxFlash_FlashType flashType)
{
    IfxFlash_FlashType resolvedFlashType;
    uint32 words[8];
    uint8 ret;

    (void)flash;
    (void)flashType;

    if (pData == NULL_PTR)
    {
        return FLASH_RESULT_INVALID_PFLASH_RANGE;
    }

    ret = (uint8)SotaFlash_ValidatePflashWrite(page_addr, 32u);
    if (ret != FLASH_RESULT_OK)
    {
    	return ret;
    }

    if (SotaTc37x_GetPFlashType(page_addr, &resolvedFlashType) == FALSE)
    {
        return FLASH_RESULT_INVALID_PFLASH_RANGE;
    }

    memcpy(words, pData, sizeof(words));

    SotaFlash_CopyPflashRoutinesToPspr();
    return sotaFlashPsprCommands.programPflashPage32(page_addr, words, resolvedFlashType);
}

uint8 SotaFlash_ProgramPage8(uint32 flash, uint32 page_addr, uint8 *pData, IfxFlash_FlashType flashType)
{
    (void)flash;
    (void)page_addr;
    (void)pData;
    (void)flashType;

    return FLASH_RESULT_DFLASH_PROGRAM_UNSUPPORTED;
}

uint8 SotaFlash_ProgramDflashPage8(uint32 pageAddr, const uint32 data[2])
{
#if (SOTA_ENABLE_UCB_WRITES != 0u)
    uint32 words[2];

    if (data == NULL_PTR)
    {
        return FLASH_RESULT_INVALID_DFLASH_RANGE;
    }

    if ((pageAddr & (IFXFLASH_DFLASH_PAGE_LENGTH - 1u)) != 0u)
    {
        return FLASH_RESULT_INVALID_DFLASH_RANGE;
    }

    if ((Flash_IsTc37xUcbRange(pageAddr, IFXFLASH_DFLASH_PAGE_LENGTH) == FALSE) &&
        (Flash_IsTc37xDflashRange(pageAddr, IFXFLASH_DFLASH_PAGE_LENGTH) == FALSE))
    {
        return FLASH_RESULT_INVALID_DFLASH_RANGE;
    }

    words[0] = data[0];
    words[1] = data[1];

    SotaFlash_CopyPflashRoutinesToPspr();
    return sotaFlashPsprCommands.programDflashPage8(pageAddr, words, IfxFlash_FlashType_D0);
#else
    (void)pageAddr;
    (void)data;
    return FLASH_RESULT_UCB_WRITE_DISABLED;
#endif
}

uint8 SotaFlash_ProgramUcb(uint32 flash, uint8 *pData, uint8 ucb_no)
{
#if (SOTA_ENABLE_UCB_WRITES != 0u)
	uint32 start_address;
	uint32 words[2];
	uint16 offset;
	uint8 ret = FLASH_RESULT_OK;

	(void)flash;

	if (pData == NULL_PTR)
	{
		return FLASH_RESULT_INVALID_DFLASH_RANGE;
	}

	if (ucb_no >= IFXFLASH_DFLASH_NUM_UCB_LOG_SECTORS)
	{
		return FLASH_RESULT_INVALID_DFLASH_RANGE;
	}

	if (Flash_IsUcbWriteEnabled(ucb_no) == FALSE)
	{
		return FLASH_RESULT_UCB_WRITE_DISABLED;
	}

	start_address = IfxFlash_dFlashTableUcbLog[ucb_no].start;

	for (offset = 0u; offset < 0x200u; offset += 8u)
	{
		memcpy(words, (pData + offset), sizeof(words));
		ret = SotaFlash_ProgramDflashPage8((start_address + offset), words);
		if(ret != FLASH_RESULT_OK)
		{
			break;
		}
	}

	return ret;
#else
	(void)flash;
	(void)pData;
	(void)ucb_no;
	return FLASH_RESULT_UCB_WRITE_DISABLED;
#endif
}

uint8 SotaFlash_ProgramUcbSwapEntry(uint32 flash, uint8 *pData, uint8 ucb_no, uint8 entry_no)
{
	(void)flash;
	(void)pData;
	(void)ucb_no;
	(void)entry_no;
	return FLASH_RESULT_UCB_WRITE_DISABLED;
}

uint8 SotaFlash_EraseUcb(uint32 flash, uint8 ucb_no)
{
#if ((SOTA_ENABLE_UCB_WRITES != 0u) && \
     (SOTA_ENABLE_UCB_SWAP_WRITES != 0u) && \
     (SOTA_ENABLE_UCB_SWAP_ERASE_REINIT != 0u))
	uint32 start_address;
	uint32 end_address;
	uint32 ucb_size;

	(void)flash;

	if (ucb_no >= IFXFLASH_DFLASH_NUM_UCB_LOG_SECTORS)
	{
		return FLASH_RESULT_INVALID_DFLASH_RANGE;
	}

	if ((ucb_no != UCB_SWAP_ORIG_NO) && (ucb_no != UCB_SWAP_COPY_NO))
	{
		return FLASH_RESULT_UCB_WRITE_DISABLED;
	}

	start_address = IfxFlash_dFlashTableUcbLog[ucb_no].start;
	end_address = IfxFlash_dFlashTableUcbLog[ucb_no].end;

	if (end_address < start_address)
	{
		return FLASH_RESULT_INVALID_DFLASH_RANGE;
	}

	ucb_size = end_address - start_address + 1u;
	if (Flash_IsTc37xUcbRange(start_address, ucb_size) == FALSE)
	{
		return FLASH_RESULT_INVALID_DFLASH_RANGE;
	}

	SotaFlash_CopyPflashRoutinesToPspr();
	return sotaFlashPsprCommands.erasePflashSector(start_address, IfxFlash_FlashType_D0);
#else
	(void)flash;
	(void)ucb_no;
	return FLASH_RESULT_UCB_WRITE_DISABLED;
#endif
}

uint8 SotaFlash_ReadUcb(uint32 flash, uint8 *pData, uint8 ucb_no, uint32 size)
{
	uint32 start_address;
	uint32 end_address;
	uint32 ucb_size;

	(void)flash;

	if ((pData == NULL_PTR) || (size == 0u))
	{
		return FLASH_RESULT_INVALID_DFLASH_RANGE;
	}

	if (ucb_no >= IFXFLASH_DFLASH_NUM_UCB_LOG_SECTORS)
	{
		return FLASH_RESULT_INVALID_DFLASH_RANGE;
	}

	start_address = IfxFlash_dFlashTableUcbLog[ucb_no].start;
	end_address = IfxFlash_dFlashTableUcbLog[ucb_no].end;

	if (end_address < start_address)
	{
		return FLASH_RESULT_INVALID_DFLASH_RANGE;
	}

	ucb_size = end_address - start_address + 1u;
	if (size > ucb_size)
	{
		return FLASH_RESULT_INVALID_DFLASH_RANGE;
	}

	memcpy(pData, (uint8*)start_address, size);

	return FLASH_RESULT_OK;
}

FlashResult SotaFlash_ValidatePflashWrite(uint32 addr, uint32 len)
{
    IfxFlash_FlashType type;
    uint8 result;

    if (SotaTc37x_GetPFlashType(addr, &type) == FALSE)
    {
        return FLASH_RESULT_INVALID_PFLASH_RANGE;
    }

    result = Flash_ValidateTc37xProgramFlashRequest(addr, len, type);

    return (FlashResult)result;
}
