#include "App_Lamp.h"
#include "adc.h"
#include "pwm.h"
#include "UART.h"
#include "IfxPort.h"

//#define AUTOMODE

volatile static uint16 brightness;
volatile static uint8 onoff;
#ifdef AUTOMODE
volatile static uint16 sensorval;
#endif

void lampinit(void)
{
  init_GTM_PWM3_TOUT104();
  IfxPort_setPinModeOutput(&MODULE_P10, 1, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
  IfxPort_setPinLow(&MODULE_P10, 1);
#ifdef AUTOMODE
  init_VADC_Group3_Ch1();
#else
  IfxPort_setPinModeInput(&MODULE_P02, 0, IfxPort_InputMode_noPullDevice);
  IfxPort_setPinModeInput(&MODULE_P02, 1, IfxPort_InputMode_noPullDevice);
#endif
}

void lamp10mstask(void)
{
#ifdef AUTOMODE
  read_EVADC_Values31(&sensorval);
  onoff = (sensorval < 2000);
#else
  if (!IfxPort_getPinState(&MODULE_P02, 0))
    onoff = 1;
  else if (!IfxPort_getPinState(&MODULE_P02, 1))
    onoff = 0;
#endif

  const uint16 db = 60;
  if (onoff)
  {
    if (brightness < 3000)
      brightness += db;
  }
  else
  {
    if (brightness > 0)
      brightness -= db;
  }

  PWM_setDutyCycle(brightness);
}
