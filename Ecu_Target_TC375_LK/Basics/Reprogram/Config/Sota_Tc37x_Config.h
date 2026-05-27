#ifndef SOTA_TC37X_CONFIG_H_
#define SOTA_TC37X_CONFIG_H_

#include "Ifx_Types.h"

#ifndef SOTA_TC37X_PFLASH_BANK0_SYSTEM_START
#ifdef SOTA_FLASH_BANK0_START_ADDR
#define SOTA_TC37X_PFLASH_BANK0_SYSTEM_START SOTA_FLASH_BANK0_START_ADDR
#else
#define SOTA_TC37X_PFLASH_BANK0_SYSTEM_START (0xA0000000U)
#endif
#endif

#ifndef SOTA_TC37X_PFLASH_BANK1_SYSTEM_START
#ifdef SOTA_FLASH_BANK1_START_ADDR
#define SOTA_TC37X_PFLASH_BANK1_SYSTEM_START SOTA_FLASH_BANK1_START_ADDR
#else
#define SOTA_TC37X_PFLASH_BANK1_SYSTEM_START (0xA0300000U)
#endif
#endif

#ifndef SOTA_TC37X_PFLASH_BANK_SIZE_BYTE
#ifdef SOTA_FLASH_BANK_SIZE_BYTE
#define SOTA_TC37X_PFLASH_BANK_SIZE_BYTE     SOTA_FLASH_BANK_SIZE_BYTE
#else
#define SOTA_TC37X_PFLASH_BANK_SIZE_BYTE     (0x00300000U)
#endif
#endif

#ifndef SOTA_TC37X_PFLASH_PAGE_SIZE_BYTE
#ifdef SOTA_FLASH_PFLASH_PAGE_SIZE_BYTE
#define SOTA_TC37X_PFLASH_PAGE_SIZE_BYTE     SOTA_FLASH_PFLASH_PAGE_SIZE_BYTE
#else
#define SOTA_TC37X_PFLASH_PAGE_SIZE_BYTE     (32U)
#endif
#endif

#ifndef SOTA_TC37X_PFLASH_SECTOR_SIZE_BYTE
#ifdef SOTA_FLASH_PFLASH_SECTOR_SIZE_BYTE
#define SOTA_TC37X_PFLASH_SECTOR_SIZE_BYTE   SOTA_FLASH_PFLASH_SECTOR_SIZE_BYTE
#else
#define SOTA_TC37X_PFLASH_SECTOR_SIZE_BYTE   (0x4000U)
#endif
#endif

#ifndef SOTA_TC37X_ACTIVE_BANK_OVERRIDE
#ifdef SOTA_FLASH_ACTIVE_BANK_OVERRIDE
#define SOTA_TC37X_ACTIVE_BANK_OVERRIDE      SOTA_FLASH_ACTIVE_BANK_OVERRIDE
#else
#define SOTA_TC37X_ACTIVE_BANK_OVERRIDE      (0xFFU)
#endif
#endif

#ifndef SOTA_TC37X_ENABLE_ADDRESS_BASED_ACTIVE_BANK_DETECT
#define SOTA_TC37X_ENABLE_ADDRESS_BASED_ACTIVE_BANK_DETECT (0U)
#endif

#ifndef SOTA_TC37X_ENABLE_SWAPCTRL_ACTIVE_BANK_DETECT
#define SOTA_TC37X_ENABLE_SWAPCTRL_ACTIVE_BANK_DETECT (1U)
#endif

#ifndef SOTA_LOCAL_ECU_TYPE_MOTOR
#define SOTA_LOCAL_ECU_TYPE_MOTOR (1U)
#endif

#ifndef SOTA_LOCAL_ECU_TYPE_STEERING
#define SOTA_LOCAL_ECU_TYPE_STEERING (0U)
#endif

// CPU1, CPU2 EN/DISABLE
#define IFX_CFG_SSW_ENABLE_TRICORE1 (0U)
#define IFX_CFG_SSW_ENABLE_TRICORE2 (0U)

typedef enum
{
    SOTA_TC37X_PFLASH_BANK_PF0 = 0,
    SOTA_TC37X_PFLASH_BANK_PF1 = 1,
    SOTA_TC37X_PFLASH_BANK_UNKNOWN = 0xFF
} SotaTc37x_PflashBank_t;

typedef enum
{
    SOTA_TC37X_RANGE_STATUS_OK = 0,
    SOTA_TC37X_RANGE_STATUS_INVALID_ADDRESS,
    SOTA_TC37X_RANGE_STATUS_INVALID_LENGTH,
    SOTA_TC37X_RANGE_STATUS_ACTIVE_BANK_ACCESS
} SotaTc37x_RangeStatus_t;

/*
 * Flash command sequences use TC37x system/physical PFLASH addresses
 * (0xA0000000 alias), not arbitrary cached pointers. Callers may pass cached
 * 0x80000000 aliases; these APIs normalize them before range checks.
 */
uint32 SotaTc37x_NormalizePflashAddress(uint32 address);
SotaTc37x_PflashBank_t SotaTc37x_GetBankByAddress(uint32 address);
SotaTc37x_PflashBank_t SotaTc37x_GetCurrentActiveBank(void);
SotaTc37x_PflashBank_t SotaTc37x_GetInactiveBank(void);
uint32 SotaTc37x_GetBankStart(SotaTc37x_PflashBank_t bank);
uint32 SotaTc37x_GetBankEnd(SotaTc37x_PflashBank_t bank);
uint32 SotaTc37x_GetBankSize(SotaTc37x_PflashBank_t bank);
uint32 SotaTc37x_GetInactiveBankStart(void);
uint32 SotaTc37x_GetInactiveBankSize(void);
boolean SotaTc37x_IsRangeInsideInactiveBank(uint32 address, uint32 length);
SotaTc37x_RangeStatus_t SotaTc37x_ValidateInactiveBankRange(uint32 address,
                                                            uint32 length);

#endif /* SOTA_TC37X_CONFIG_H_ */
