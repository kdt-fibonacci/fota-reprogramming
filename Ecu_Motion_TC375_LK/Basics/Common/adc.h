#ifndef _ADC_H_
#define _ADC_H_

#include <Ifx_Types.h>

#ifndef JOYSTICK_E_
#define JOYSTICK_E_
typedef enum
{
  JOY_NEUTRAL,
  JOY_UP,
  JOY_DOWN,
  JOY_LEFT,
  JOY_RIGHT
} joystick_dir_e;
#endif

void init_VADC_Group3_Ch0_Ch1(void);
void init_VADC_Group4_Ch4_Ch5(void);
uint16 get_motor_current_adc(void);
joystick_dir_e Joystick_read(void);
sint8 Joystick_read_level(void);
void setneutral(void);

#endif