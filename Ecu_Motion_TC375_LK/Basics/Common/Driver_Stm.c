#include "Driver_Stm.h"

#define STM_PERIOD_MS  (1U)

volatile SchedulingFlag stSchedulingInfo;
static App_stm g_Stm;
static volatile unsigned int u32nuCounter1ms = 0u;
static uint32 g_stmTicks1ms;

void initSTM(void)
{
    boolean interruptState = IfxCpu_disableInterrupts();

    g_Stm.stmSfr = &MODULE_STM0;
    IfxStm_initCompareConfig(&g_Stm.stmConfig);

    g_stmTicks1ms = IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, STM_PERIOD_MS);

    g_Stm.stmConfig.triggerPriority = 0x30u;
    g_Stm.stmConfig.typeOfService   = IfxSrc_Tos_cpu0;
    g_Stm.stmConfig.ticks           = g_stmTicks1ms;

    IfxStm_initCompare(g_Stm.stmSfr, &g_Stm.stmConfig);

    IfxCpu_restoreInterrupts(interruptState);
}

void Driver_Stm_Init(void)
{
    stSchedulingInfo.u8nuScheduling1msFlag   = 0u;
    stSchedulingInfo.u8nuScheduling10msFlag  = 0u;
    stSchedulingInfo.u8nuScheduling100msFlag = 0u;
    stSchedulingInfo.u8nuScheduling1000msFlag = 0u;

    u32nuCounter1ms = 0u;

    initSTM();
}

IFX_INTERRUPT(STM_Int0Handler, 0, 0x30);
void STM_Int0Handler(void)
{
    IfxCpu_enableInterrupts();

    IfxStm_clearCompareFlag(g_Stm.stmSfr, g_Stm.stmConfig.comparator);
    IfxStm_increaseCompare(g_Stm.stmSfr, g_Stm.stmConfig.comparator, g_stmTicks1ms);

    u32nuCounter1ms++;
    stSchedulingInfo.u8nuScheduling1msFlag = 1u;

    if ((u32nuCounter1ms % 10u) == 0u)   stSchedulingInfo.u8nuScheduling10msFlag = 1u;
    if ((u32nuCounter1ms % 100u) == 0u)  stSchedulingInfo.u8nuScheduling100msFlag = 1u;
    if ((u32nuCounter1ms % 1000u) == 0u) stSchedulingInfo.u8nuScheduling1000msFlag = 1u;
}
