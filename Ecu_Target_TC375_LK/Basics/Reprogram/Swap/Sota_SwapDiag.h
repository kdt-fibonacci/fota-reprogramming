#ifndef SOTA_SWAPDIAG_H_
#define SOTA_SWAPDIAG_H_

#include "Ifx_Types.h"

#include "Sota_SwapManager.h"

SotaSwapManager_Result_t SotaSwapDiag_AppendNextSwapEntry(uint8 targetBank,
                                                          uint16 *outEntryIndex);
SotaSwapManager_Result_t SotaSwapDiag_FactoryEnableOtp(void);
SotaSwapManager_Result_t SotaSwapDiag_FactoryEraseAndReinitSwap(void);

#endif /* SOTA_SWAPDIAG_H_ */
