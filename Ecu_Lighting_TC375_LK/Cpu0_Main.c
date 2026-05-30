/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

#include "Std_Types.h"
#include "ComStack_Types.h"

#include "UART.h"

#include "Can.h"
#include "CanIf.h"
#include "CanTp.h"
#include "PduR.h"
#include "Dcm.h"

#include "FotaHandler.h"

#include "Time.h"
#include "Can_Cfg.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

#define TEST_MAIN_PERIOD_MS                  (1U)

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

    UART_Printf("\r\n");
    UART_Printf("========================================\r\n");
    UART_Printf("[Lighting] Target DCM Responder Start\r\n");
    UART_Printf("========================================\r\n");

    while (1)
    {
        Test_MainFunctions();

        Shared_Util_Time_DelayMs(1);
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
    Dcm_Init();
    FOTA_ProvisionInitialOnce();
}

static void Test_MainFunctions(void)
{
    /*
     * 순서는 크게 아래 흐름이면 된다.
     *
     * 1. CAN Rx frame 처리
     * 2. CanTp 재조립/송신 상태머신 처리
     * 3. DCM pending request 처리
     * 4. CAN Tx pending frame 처리
     */
    Can_MainFunction_Read();

    CanTp_MainFunction();

    Dcm_MainFunction();

    Can_MainFunction_Write();

    FOTAHandlerMain();
}
