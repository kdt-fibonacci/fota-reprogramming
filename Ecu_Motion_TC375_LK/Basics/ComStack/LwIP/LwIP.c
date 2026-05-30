#include "LWIP.h"
#include "LWIP_Cfg.h"

#include "IfxGeth_Eth.h"
#include "Ifx_Lwip.h"
#include "Configuration.h"

void LwIP_TimerInit(void)
{
    IfxStm_CompareConfig StmCompareConfig;

    IfxStm_initCompareConfig(&StmCompareConfig);

    StmCompareConfig.triggerPriority     = ISR_PRIORITY_OS_TICK;
    StmCompareConfig.comparatorInterrupt = IfxStm_ComparatorInterrupt_ir0;
    StmCompareConfig.ticks               = IFX_CFG_STM_TICKS_PER_MS * 10U;
    StmCompareConfig.typeOfService       = IfxSrc_Tos_cpu0;

    IfxStm_initCompare(
        &MODULE_STM0,
        &StmCompareConfig
    );
}

void LwIP_Init(void)
{
    IfxGeth_enableModule(&MODULE_GETH);

    LwIP_TimerInit();

    Ifx_Lwip_init(EthAddr);
}

void LwIP_MainFunction(void)
{
    Ifx_Lwip_pollTimerFlags();
    Ifx_Lwip_pollReceiveFlags();
}

IFX_INTERRUPT(updateLwIPStackISR, 0, ISR_PRIORITY_OS_TICK);
void updateLwIPStackISR(void)
{
    IfxStm_increaseCompare(
        &MODULE_STM0,
        IfxStm_Comparator_0,
        IFX_CFG_STM_TICKS_PER_MS
    );

    g_TickCount_1ms++;

    Ifx_Lwip_onTimerTick();
}