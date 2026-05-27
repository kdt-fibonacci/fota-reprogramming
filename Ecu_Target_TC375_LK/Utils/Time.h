#ifndef TIME_H_
#define TIME_H_

#include "Platform_Types.h"

uint32 Shared_Util_Time_GetNowMs(void);
uint64 Shared_Util_Time_GetNowUs(void);
void Shared_Util_Time_DelayMs(uint32 delayMs);
void Shared_Util_Time_DelayUs(uint32 delayUs);

#endif
