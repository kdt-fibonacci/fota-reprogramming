/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "IfxCpu.h"
#include "IfxScuWdt.h"

#include "Std_Types.h"
#include "ComStack_Types.h"

#include "UART.h"

#include "Can.h"
#include "CanIf.h"
#include "CanTp.h"
#include "PduR.h"
#include "LwIP.h"
#include "DoIP.h"
#include "SoAd.h"
#include "Dcm.h"

#include "Can_Cfg.h"

#include "Shared_Util_Time.h"

/*********************************************************************************************************************/
/*------------------------------------------------Global Variables---------------------------------------------------*/
/*********************************************************************************************************************/

IfxCpu_syncEvent g_cpuSyncEvent;

/*
 * Gateway에서 Target 응답이 다시 DoIP_TpTransmit()까지 올라왔는지 확인하기 위한 테스트 플래그.
 *
 * 주의:
 * 이 변수는 테스트용이다.
 * DoIP_TpTransmit() 내부에서 Diagnostic Message Response를 생성한 뒤 TRUE로 세팅해줘야 한다.
 */
volatile boolean Test_DoIPResponseReceived = FALSE;

/*********************************************************************************************************************/
/*------------------------------------------------Static Variables---------------------------------------------------*/
/*********************************************************************************************************************/

static volatile uint32 Test_Tick1ms = 0U;


/*********************************************************************************************************************/
/*------------------------------------------------Private Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static void Test_InitModules(void);
static void Test_MainFunctions(void);

/*********************************************************************************************************************/
/*------------------------------------------------Main Function------------------------------------------------------*/
/*********************************************************************************************************************/

void core0_main(void)
{
    IfxCpu_enableInterrupts();

    IfxScuWdt_disableCpuWatchdog(
        IfxScuWdt_getCpuWatchdogPassword()
    );

    IfxScuWdt_disableSafetyWatchdog(
        IfxScuWdt_getSafetyWatchdogPassword()
    );

    Test_InitModules();

    UART_Printf("========================================\r\n");
    UART_Printf("[GW] Gateway FOTA UDS Sequence Test Start\r\n");
    UART_Printf("========================================\r\n");

    while (1)
    {
        Test_MainFunctions();

        Shared_Util_Time_DelayMs(1);

        Test_Tick1ms++;
    }
}

/*********************************************************************************************************************/
/*------------------------------------------------Module Init/Main---------------------------------------------------*/
/*********************************************************************************************************************/

static void Test_InitModules(void)
{
    UART_Init();

    Can_Init();

    (void)Can_SetControllerMode(
        CAN_CONTROLLER_0,
        CAN_CS_STARTED
    );

    CanIf_Init();
    CanTp_Init();

    PduR_Init();
    LwIP_Init();
    DoIP_Init();
    SoAd_Init();
    Dcm_Init();
}

static void Test_MainFunctions(void)
{
    Can_MainFunction_Read();
    CanTp_MainFunction();
    Can_MainFunction_Write();

    LwIP_MainFunction();
    Dcm_MainFunction();
}
