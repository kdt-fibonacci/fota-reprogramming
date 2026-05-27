#ifndef SOTA_SWAPMANAGER_H_
#define SOTA_SWAPMANAGER_H_

#include "Ifx_Types.h"

#include "../Config/Sota_Tc37x_Config.h"

#ifndef SOTA_ENABLE_FACTORY_PROVISIONING
#define SOTA_ENABLE_FACTORY_PROVISIONING (0U)
#endif

#ifndef SOTA_SWAPMANAGER_USE_SWAPDIAG
#define SOTA_SWAPMANAGER_USE_SWAPDIAG (1U)
#endif

#ifndef SOTA_SWAPMANAGER_REQUIRE_RUNTIME_PORT
#define SOTA_SWAPMANAGER_REQUIRE_RUNTIME_PORT (0U)
#endif

#if ((SOTA_SWAPMANAGER_REQUIRE_RUNTIME_PORT == 1U) && \
     (SOTA_SWAPMANAGER_USE_SWAPDIAG != 1U))
#error "SOTA runtime swap append port is required but not enabled"
#endif

typedef enum
{
    SOTA_SWAP_MANAGER_RESULT_OK = 0,
    SOTA_SWAP_MANAGER_RESULT_INVALID_PARAM,
    SOTA_SWAP_MANAGER_RESULT_INVALID_STATE,
    SOTA_SWAP_MANAGER_RESULT_NOT_CONFIGURED,
    SOTA_SWAP_MANAGER_RESULT_WRITE_ERROR,
    SOTA_SWAP_MANAGER_RESULT_VERIFY_ERROR,
    SOTA_SWAP_MANAGER_RESULT_NO_FREE_ENTRY
} SotaSwapManager_Result_t;

void SotaSwapManager_Init(void);
SotaTc37x_PflashBank_t SotaSwapManager_GetCurrentBank(void);
SotaTc37x_PflashBank_t SotaSwapManager_GetInactiveBank(void);
SotaSwapManager_Result_t SotaSwapManager_ArmSwapToBank(SotaTc37x_PflashBank_t targetBank);
SotaSwapManager_Result_t SotaSwapManager_ArmRollbackToPreviousBank(void);
uint16 SotaSwapManager_GetLastSwapEntryIndex(void);

#if (SOTA_ENABLE_FACTORY_PROVISIONING == 1U)
SotaSwapManager_Result_t SotaSwapManager_FactoryEnableOtp(void);
SotaSwapManager_Result_t SotaSwapManager_FactoryEraseAndReinitSwap(void);
#endif

#endif /* SOTA_SWAPMANAGER_H_ */
