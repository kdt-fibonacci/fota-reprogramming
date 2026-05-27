#include <Time.h>
#include "IfxStm.h"

uint32 Shared_Util_Time_GetNowMs(void)
{
    uint64 ticks = IfxStm_get(&MODULE_STM0);
    uint64 freq  = (uint64)IfxStm_getFrequency(&MODULE_STM0);

    return (uint32)(ticks / (freq / 1000ULL));
}

uint64 Shared_Util_Time_GetNowUs(void)
{
    uint64 ticks = IfxStm_get(&MODULE_STM0);
    uint64 freq  = (uint64)IfxStm_getFrequency(&MODULE_STM0);

    return (uint32)(ticks / (freq / 1000000ULL));
}

void Shared_Util_Time_DelayMs(uint32 delayMs)
{
    uint32 start = Shared_Util_Time_GetNowMs();

    while ((uint32)(Shared_Util_Time_GetNowMs() - start) < delayMs)
    {
        /* busy wait */
    }
}

void Shared_Util_Time_DelayUs(uint32 delayUs)
{
    uint64 start = Shared_Util_Time_GetNowUs();

    while ((uint64)(Shared_Util_Time_GetNowUs() - start) < (uint64)delayUs)
    {
        /* busy wait */
    }
}
