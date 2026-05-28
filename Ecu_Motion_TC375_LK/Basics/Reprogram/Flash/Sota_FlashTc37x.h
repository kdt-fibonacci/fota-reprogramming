/**
 * \file Sota_FlashTc37x.h
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
 *
 * \defgroup IfxLld_Demo_FlashDemo_SrcDoc_Main Demo Source
 * \ingroup IfxLld_Demo_FlashDemo_SrcDoc
 * \defgroup IfxLld_Demo_FlashDemo_SrcDoc_Main_Interrupt Interrupts
 * \ingroup IfxLld_Demo_FlashDemo_SrcDoc_Main
 */

#ifndef SOTA_FLASH_TC37X_H
#define SOTA_FLASH_TC37X_H 1

/******************************************************************************/
/*----------------------------------Includes----------------------------------*/
/******************************************************************************/
#include <Ifx_Types.h>
#include <IfxFlash.h>

/******************************************************************************/
/*-----------------------------------Macros-----------------------------------*/
/******************************************************************************/
#define UCB_SWAP_ORIG_NO   23
#define UCB_SWAP_COPY_NO   31

#define UCB_OTP_ORIG_NO         32
#define UCB_OTP_COPY_NO         40

#define UCB_UNLOCKED_CODE     0x43211234
#define UCB_CONFIRMED_CODE    0x57B5327F

#define SOTA_FLASH_HAS_VALIDATE_PFLASH_WRITE 1

/******************************************************************************/
/*--------------------------------Enumerations--------------------------------*/
/******************************************************************************/
typedef enum
{
    FLASH_RESULT_OK = 0u,
    FLASH_RESULT_INVALID_FLASH = 1u,
    FLASH_RESULT_UNSUPPORTED_FLASH_TYPE = 2u,
    FLASH_RESULT_INVALID_PFLASH_RANGE = 3u,
    FLASH_RESULT_ACTIVE_BANK = 4u,
    FLASH_RESULT_BANK_BOUNDARY = 5u,
    FLASH_RESULT_FLASH_TYPE_MISMATCH = 6u,
    FLASH_RESULT_ENTER_PAGE_MODE_FAILED = 7u,
    FLASH_RESULT_DMU_ERROR = 8u,
    FLASH_RESULT_UCB_WRITE_DISABLED = 9u,
    FLASH_RESULT_DFLASH_PROGRAM_UNSUPPORTED = 10u,
    FLASH_RESULT_ACTIVE_BANK_UNKNOWN = 11u,
    FLASH_RESULT_INVALID_DFLASH_RANGE = 12u
} FlashResult;

/******************************************************************************/
/*-----------------------------Data Structures--------------------------------*/
/******************************************************************************/

/******************************************************************************/
/*-------------------------Function Prototypes--------------------------------*/
/******************************************************************************/
extern uint8 SotaFlash_EraseSector(uint32 flash, uint32 sector_addr, IfxFlash_FlashType flashType);
extern uint8 SotaFlash_ProgramPage256(uint32 flash, uint32 page_addr, uint8 *pData, IfxFlash_FlashType flashType);
extern uint8 SotaFlash_ProgramPage32(uint32 flash, uint32 page_addr, uint8 *pData, IfxFlash_FlashType flashType);
extern uint8 SotaFlash_ProgramPage8(uint32 flash, uint32 page_addr, uint8 *pData, IfxFlash_FlashType flashType);
extern uint8 SotaFlash_ProgramDflashPage8(uint32 pageAddr, const uint32 data[2]);
extern uint8 SotaFlash_ProgramUcb(uint32 flash, uint8 *pData, uint8 ucb_no);
extern uint8 SotaFlash_ProgramUcbSwapEntry(uint32 flash, uint8 *pData, uint8 ucb_no, uint8 entry_no);
extern uint8 SotaFlash_EraseUcb(uint32 flash, uint8 ucb_no);
extern uint8 SotaFlash_ReadUcb(uint32 flash, uint8 *pData, uint8 ucb_no, uint32 size);
extern void SotaFlash_CopyPflashRoutinesToPspr(void);
FlashResult SotaFlash_ValidatePflashWrite(uint32 addr, uint32 len);

#endif /* SOTA_FLASH_TC37X_H */
