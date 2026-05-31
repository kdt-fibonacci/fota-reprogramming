#include "accel.h"
#include "adc.h"
#include "pwm.h"

#include "IfxPort.h"

//#define ENABLE_TCS

void motor_init(void)
{
  // SNSA
  init_VADC_Group3_Ch0_Ch1();

  // SNSB
  init_VADC_Group4_Ch4_Ch5();

  // PWMA & PWMB
  init_GTM_PWM3_TOUT1();

  // DIRA
  IfxPort_setPinModeOutput(&MODULE_P10, 1, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
  IfxPort_setPinLow(&MODULE_P10, 1);

  // DIRB
  IfxPort_setPinModeOutput(&MODULE_P10, 2, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
  IfxPort_setPinHigh(&MODULE_P10, 2);

  setneutral();
}

volatile static int speed;
// th: 0~100
void setThrottle(void)
{
  uint8 th, dir;
  sint8 joyres = Joystick_read_level();
  volatile static float duty = 0;
  volatile static float duty2 = 0;

  th = joyres > 0 ? joyres : -joyres;
  dir = joyres == 0 ? 0 : (joyres > 0 ? 1 : 2);

#define TCS_ENABLE
#ifdef TCS_ENABLE
  if (dir == 0)
    duty = 0.98f * duty;
  else if (dir == 1)
    duty = 0.80f * duty + 100.0f * th;
  else
    duty = 0.90f * duty - 50.0f * th;
#else
  if (dir == 0)
    duty = 0.99f * duty;
  else if (dir == 1)
    duty = 0.10f * duty + 450.0f * th;
  else
    duty = 0.90f * duty - 50.0f * th;
#endif

  if (dir != 1 && duty < 17000) duty = 0;
  else if (duty > 49999) duty = 49999;
  duty2 = duty;
  speed = duty2;

#ifdef TCS_ENABLE
  // if (duty2 > 25000 && get_motor_current_adc() < 200) duty2 = 25000;
#endif

  PWM_setDutyCycle((uint16)duty2);
}

boolean isStopped(void)
{
  return speed == 0;
}
