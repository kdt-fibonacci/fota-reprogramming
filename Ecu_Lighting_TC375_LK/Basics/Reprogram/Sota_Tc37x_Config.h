/*
 * Sota_Tc37x_Config.h
 *
 * TC37x/TC375 SOTA flash layout configuration.
 */

#ifndef SOTA_TC37X_CONFIG_H
#define SOTA_TC37X_CONFIG_H 1

/******************************************************************************/
/*----------------------------------Includes----------------------------------*/
/******************************************************************************/
#include <Ifx_Types.h>
#include <IfxFlash.h>

/******************************************************************************/
/*-----------------------------------Macros-----------------------------------*/
/******************************************************************************/
#ifndef SOTA_TC37X_TARGET
#define SOTA_TC37X_TARGET               (1u)
#endif

#define TC37X_PF0_NC_START              (0xA0000000u)
#define TC37X_PF0_NC_END                (0xA02FFFFFu)
#define TC37X_PF1_NC_START              (0xA0300000u)
#define TC37X_PF1_NC_END                (0xA05FFFFFu)
#define TC37X_PFLASH_BANK_SIZE          (0x00300000u)
#define TC37X_PFLASH_SECTOR_SIZE        (0x00004000u)

#define TC37X_PFLASH_NC_INVALID_START   (0xA0600000u)

#define SOTA_INVALID_ADDRESS            (0xFFFFFFFFu)

#ifndef SOTA_ENABLE_INITIAL_PROVISIONING
#define SOTA_ENABLE_INITIAL_PROVISIONING    (1u)
#endif

#ifndef SOTA_ENABLE_UCB_WRITES
#define SOTA_ENABLE_UCB_WRITES              (1u)
#endif

#ifndef SOTA_ENABLE_UCB_SWAP_WRITES
#define SOTA_ENABLE_UCB_SWAP_WRITES         (1u)
#endif

#ifndef SOTA_ENABLE_UCB_OTP_WRITES
#define SOTA_ENABLE_UCB_OTP_WRITES          (1u)
#endif

#ifndef SOTA_ENABLE_UCB_SWAP_ERASE_REINIT
#define SOTA_ENABLE_UCB_SWAP_ERASE_REINIT   (1u)
#endif

#ifndef SOTA_ENABLE_AUTO_RESET_AFTER_VERIFY
#define SOTA_ENABLE_AUTO_RESET_AFTER_VERIFY (0u)
#endif

#if (SOTA_ENABLE_AUTO_RESET_AFTER_VERIFY != 0u)
#error "Auto reset forbidden"
#endif

#define SOTA_UCB_SWAP_STANDARD              (0x00000055u)
#define SOTA_UCB_SWAP_ALTERNATE             (0x000000AAu)
#define SOTA_UCB_CONFIRM_CODE               (0x57B5327Fu)
#define SOTA_UCB_UNLOCKED_CODE              (0x43211234u)
/*
 * 0x000F0000 enables TC37x SOTA support in UCB_OTP.PROCONTP:
 * SWAPEN[17:16] = 11b, CPU0DDIS bit18 = 1, CPU1DDIS bit19 = 1.
 * Bits 20 and above are reserved here and must not be modified by this mask.
 */
#define SOTA_TC37X_PROCONTP_SOTA_MASK       (0x000F0000u)

#if (SOTA_TC37X_TARGET != 0u)
#if (IFXFLASH_PFLASH_PAGE_LENGTH != 32u)
#error "TC37x PFLASH page length must be 32 bytes."
#endif
#if ((TC37X_PF0_NC_END - TC37X_PF0_NC_START + 1u) != TC37X_PFLASH_BANK_SIZE)
#error "TC37x PF0 size must be 0x00300000."
#endif
#if ((TC37X_PF1_NC_END - TC37X_PF1_NC_START + 1u) != TC37X_PFLASH_BANK_SIZE)
#error "TC37x PF1 size must be 0x00300000."
#endif
#if (TC37X_PF1_NC_END >= TC37X_PFLASH_NC_INVALID_START)
#error "TC37x PF1 must end below 0xA0600000."
#endif
#endif

/******************************************************************************/
/*--------------------------------Enumerations--------------------------------*/
/******************************************************************************/
typedef enum
{
    SOTA_SWAP_STANDARD  = 0x55u,
    SOTA_SWAP_ALTERNATE = 0xAAu
} SotaSwapMode;

typedef enum
{
    SOTA_BANK_PF0 = 0u,
    SOTA_BANK_PF1 = 1u,
    SOTA_BANK_INVALID = 0xFFu
} SotaBank;

/******************************************************************************/
/*-------------------------Function Prototypes--------------------------------*/
/******************************************************************************/
extern boolean SotaTc37x_IsPf0Address(uint32 addr);
extern boolean SotaTc37x_IsPf1Address(uint32 addr);
extern boolean SotaTc37x_IsValidPflashRange(uint32 addr, uint32 len);
extern boolean SotaTc37x_GetPFlashType(uint32 addr, IfxFlash_FlashType *type);
extern SotaBank SotaTc37x_GetBankByAddress(uint32 addr);
extern uint32 SotaTc37x_GetBankStart(SotaBank bank);
extern uint32 SotaTc37x_GetBankEnd(SotaBank bank);
extern SotaBank SotaTc37x_GetActiveBank(void);
extern SotaBank SotaTc37x_GetInactiveBank(void);
extern uint32 SotaTc37x_GetInactiveBankStart(void);
extern uint32 SotaTc37x_GetInactiveBankEnd(void);

#endif /* SOTA_TC37X_CONFIG_H */
