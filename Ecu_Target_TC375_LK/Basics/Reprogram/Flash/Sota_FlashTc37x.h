#ifndef SOTA_FLASHTC37X_H_
#define SOTA_FLASHTC37X_H_

#include "Ifx_Types.h"

#include "../Config/Sota_Tc37x_Config.h"

#ifndef SOTA_FLASH_BANK0_START_ADDR
#define SOTA_FLASH_BANK0_START_ADDR        SOTA_TC37X_PFLASH_BANK0_SYSTEM_START
#endif

#ifndef SOTA_FLASH_BANK1_START_ADDR
#define SOTA_FLASH_BANK1_START_ADDR        SOTA_TC37X_PFLASH_BANK1_SYSTEM_START
#endif

#ifndef SOTA_FLASH_BANK_SIZE_BYTE
#define SOTA_FLASH_BANK_SIZE_BYTE          SOTA_TC37X_PFLASH_BANK_SIZE_BYTE
#endif

#ifndef SOTA_FLASH_PFLASH_PAGE_SIZE_BYTE
#define SOTA_FLASH_PFLASH_PAGE_SIZE_BYTE   SOTA_TC37X_PFLASH_PAGE_SIZE_BYTE
#endif

#ifndef SOTA_FLASH_PFLASH_SECTOR_SIZE_BYTE
#define SOTA_FLASH_PFLASH_SECTOR_SIZE_BYTE SOTA_TC37X_PFLASH_SECTOR_SIZE_BYTE
#endif

#ifndef SOTA_FLASH_ACTIVE_BANK_OVERRIDE
#define SOTA_FLASH_ACTIVE_BANK_OVERRIDE    SOTA_TC37X_ACTIVE_BANK_OVERRIDE
#endif

#ifndef SOTA_FLASH_BUSY_WAIT_LOOP_COUNT
#define SOTA_FLASH_BUSY_WAIT_LOOP_COUNT    (0x00FFFFFFU)
#endif

typedef enum
{
    SOTA_FLASH_RESULT_OK = 0,
    SOTA_FLASH_RESULT_INVALID_PARAM,
    SOTA_FLASH_RESULT_INVALID_ADDRESS,
    SOTA_FLASH_RESULT_INVALID_LENGTH,
    SOTA_FLASH_RESULT_INVALID_RANGE,
    SOTA_FLASH_RESULT_ACTIVE_BANK_ACCESS,
    SOTA_FLASH_RESULT_ACTIVE_BANK = SOTA_FLASH_RESULT_ACTIVE_BANK_ACCESS,
    SOTA_FLASH_RESULT_FLASH_BUSY_TIMEOUT,
    SOTA_FLASH_RESULT_HARDWARE_ERROR,
    SOTA_FLASH_RESULT_VERIFY_ERROR,
    SOTA_FLASH_RESULT_UNSUPPORTED
} SotaFlash_Result_t;

typedef enum
{
    SOTA_FLASH_BANK_PF0 = 0,
    SOTA_FLASH_BANK_PF1 = 1,
    SOTA_FLASH_BANK_UNKNOWN = 0xFF
} SotaFlash_Bank_t;

SotaFlash_Bank_t SotaFlash_GetBankByAddress(uint32 address);
SotaFlash_Bank_t SotaFlash_GetActiveBank(void);
SotaFlash_Bank_t SotaFlash_GetInactiveBank(void);
uint32 SotaFlash_GetBankStart(SotaFlash_Bank_t bank);
uint32 SotaFlash_GetBankEnd(SotaFlash_Bank_t bank);
uint32 SotaFlash_GetBankSize(SotaFlash_Bank_t bank);
uint32 SotaFlash_GetInactiveBankStart(void);
uint32 SotaFlash_GetInactiveBankEnd(void);
uint32 SotaFlash_GetInactiveBankSize(void);
uint32 SotaFlash_GetPflashPageSize(void);
uint32 SotaFlash_GetPflashSectorSize(void);
uint32 SotaFlash_NormalizePflashAddress(uint32 address);
boolean SotaFlash_IsAddressRangeInsideInactiveBank(uint32 address,
                                                   uint32 length);

SotaFlash_Result_t SotaFlash_CopyPflashRoutinesToPspr(void);
SotaFlash_Result_t SotaFlash_ValidateInactivePflashRange(uint32 address,
                                                         uint32 length);
SotaFlash_Result_t SotaFlash_EraseSector(uint32 sectorAddress);
SotaFlash_Result_t SotaFlash_ProgramPage32(uint32 pageAddress,
                                           const uint8 *pageData);
SotaFlash_Result_t SotaFlash_Read(uint32 address,
                                  uint8 *outData,
                                  uint32 length);
SotaFlash_Result_t SotaFlash_GetUcbStart(uint8 ucbIndex,
                                         uint32 *outStartAddress);
SotaFlash_Result_t SotaFlash_ReadUcb(uint8 ucbIndex,
                                     uint32 offset,
                                     uint8 *outData,
                                     uint32 length);
SotaFlash_Result_t SotaFlash_ProgramUcbSwapPage8(uint8 ucbIndex,
                                                 uint32 offset,
                                                 const uint8 *pageData);

#endif /* SOTA_FLASHTC37X_H_ */
