/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "App_Scheduler.h"
#include "IfxPort.h"

#include "accel.h"
#include "adc.h"
#include "pwm.h"

#include "Can.h"
#include "CanIf.h"
#include "CanTp.h"
#include "PduR.h"
#include "Dcm.h"

#include "FotaHandler.h"

#include "Driver_Stm.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define SPIN_BORDER 500
#define SPEED       60

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
static void App_Scheduler_Run_1ms(void);
static void App_Scheduler_Run_10ms(void);
static void App_Scheduler_Run_100ms(void);
static void App_Scheduler_Run_1s(void);
void App_Scheduler_Init(void);
void App_Scheduler_Run(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

static void App_Scheduler_Run_1ms(void)
{
    Can_MainFunction_Write();
    Can_MainFunction_Read();
    Can_MainFunction_Write();
    CanTp_MainFunction();
    Can_MainFunction_Write();
    Dcm_MainFunction();
    Can_MainFunction_Write();
    FOTAHandlerMain();
}


static void App_Scheduler_Run_10ms(void)
{
    setThrottle();
    if (isStopped())
    {
      IfxPort_setPinLow(&MODULE_P00, 5);
    }
    else
    {
      IfxPort_setPinHigh(&MODULE_P00, 5);
    }
}

static void App_Scheduler_Run_100ms(void)
{
}

static void App_Scheduler_Run_1s(void)
{
}

void App_Scheduler_Init(void)
{
  Driver_Stm_Init();
  motor_init();
  IfxPort_setPinModeOutput(&MODULE_P00, 5, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
  IfxPort_setPinHigh(&MODULE_P00, 5);
}

void App_Scheduler_Run(void)
{
  if (stSchedulingInfo.u8nuScheduling1msFlag == 1u)
  {
    stSchedulingInfo.u8nuScheduling1msFlag = 0u;
    App_Scheduler_Run_1ms();

    if (stSchedulingInfo.u8nuScheduling10msFlag == 1u)
    {
      stSchedulingInfo.u8nuScheduling10msFlag = 0u;
      App_Scheduler_Run_10ms();
    }

    if (stSchedulingInfo.u8nuScheduling100msFlag == 1u)
    {
      stSchedulingInfo.u8nuScheduling100msFlag = 0u;
      App_Scheduler_Run_100ms();
    }

    if (stSchedulingInfo.u8nuScheduling1000msFlag == 1u)
    {
      stSchedulingInfo.u8nuScheduling1000msFlag = 0u;
      App_Scheduler_Run_1s();
    }
  }
}
