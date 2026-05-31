#include "adc.h"
#include "pwm.h"

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "Evadc/Adc/IfxEvadc_Adc.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/
IfxEvadc_Adc evadc;
IfxEvadc_Adc_Group adcGroup301;
IfxEvadc_Adc_Channel adcChannel301[2];
IfxEvadc_Adc_Group adcGroup445;
IfxEvadc_Adc_Channel adcChannel445[2];

static volatile uint16 adcResult0 = 0;
static volatile uint16 adcResult1 = 0;

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void init_VADC_Group3_Ch0_Ch1(void)
{
  /* 1. EVADC 모듈 설정 및 초기화 */
  IfxEvadc_Adc_Config adcConfig;
  IfxEvadc_Adc_initModuleConfig(&adcConfig, &MODULE_EVADC);
  IfxEvadc_Adc_initModule(&evadc, &adcConfig);

  /* 2. EVADC 그룹 설정 (Group 0 사용) */
  IfxEvadc_Adc_GroupConfig adcGroupConfig;
  IfxEvadc_Adc_initGroupConfig(&adcGroupConfig, &evadc);
  adcGroupConfig.groupId = IfxEvadc_GroupId_3;
  adcGroupConfig.master = adcGroupConfig.groupId;

  // Gating을 항상 허용하여 큐가 막히지 않도록 합니다.
  adcGroupConfig.queueRequest[0].triggerConfig.gatingMode = IfxEvadc_GatingMode_always;
  // 소프트웨어 트리거로 동작하도록 설정합니다.
  adcGroupConfig.queueRequest[0].triggerConfig.triggerMode = IfxEvadc_TriggerMode_noExternalTrigger;

  // 큐 0을 사용하도록 설정합니다.
  adcGroupConfig.arbiter.requestSlotQueue0Enabled = TRUE;
  IfxEvadc_Adc_initGroup(&adcGroup301, &adcGroupConfig);

  /* 3. EVADC 채널 설정 (Channel 0, Channel 1) */
  IfxEvadc_Adc_ChannelConfig adcChannelConfig;

  // Channel 0 설정
  IfxEvadc_Adc_initChannelConfig(&adcChannelConfig, &adcGroup301);
  adcChannelConfig.channelId = IfxEvadc_ChannelId_0;
  adcChannelConfig.resultRegister = IfxEvadc_ChannelResult_0; // 결과를 저장할 전용 레지스터
  IfxEvadc_Adc_initChannel(&adcChannel301[0], &adcChannelConfig);

  // Channel 1 설정
  IfxEvadc_Adc_initChannelConfig(&adcChannelConfig, &adcGroup301);
  adcChannelConfig.channelId = IfxEvadc_ChannelId_1;
  adcChannelConfig.resultRegister = IfxEvadc_ChannelResult_1;
  IfxEvadc_Adc_initChannel(&adcChannel301[1], &adcChannelConfig);

  /* 4. 변환 큐에 채널 추가 (자동 재충전 옵션 사용) */
  IfxEvadc_Adc_addToQueue(&adcChannel301[0], IfxEvadc_RequestSource_queue0, IFXEVADC_QUEUE_REFILL);
  IfxEvadc_Adc_addToQueue(&adcChannel301[1], IfxEvadc_RequestSource_queue0, IFXEVADC_QUEUE_REFILL);

  /* 5. 큐 변환 시작 */
  IfxEvadc_Adc_startQueue(&adcGroup301, IfxEvadc_RequestSource_queue0);
}

void init_VADC_Group4_Ch4_Ch5(void)
{
  /* 1. EVADC 모듈 설정 및 초기화 */
  IfxEvadc_Adc_Config adcConfig;
  IfxEvadc_Adc_initModuleConfig(&adcConfig, &MODULE_EVADC);
  IfxEvadc_Adc_initModule(&evadc, &adcConfig);

  /* 2. EVADC 그룹 설정 (Group 0 사용) */
  IfxEvadc_Adc_GroupConfig adcGroupConfig;
  IfxEvadc_Adc_initGroupConfig(&adcGroupConfig, &evadc);
  adcGroupConfig.groupId = IfxEvadc_GroupId_8;
  adcGroupConfig.master = adcGroupConfig.groupId;

  // Gating을 항상 허용하여 큐가 막히지 않도록 합니다.
  adcGroupConfig.queueRequest[0].triggerConfig.gatingMode = IfxEvadc_GatingMode_always;
  // 소프트웨어 트리거로 동작하도록 설정합니다.
  adcGroupConfig.queueRequest[0].triggerConfig.triggerMode = IfxEvadc_TriggerMode_noExternalTrigger;

  // 큐 0을 사용하도록 설정합니다.
  adcGroupConfig.arbiter.requestSlotQueue0Enabled = TRUE;
  IfxEvadc_Adc_initGroup(&adcGroup445, &adcGroupConfig);

  /* 3. EVADC 채널 설정 (Channel 0, Channel 1) */
  IfxEvadc_Adc_ChannelConfig adcChannelConfig;

  // Channel 0 설정
  IfxEvadc_Adc_initChannelConfig(&adcChannelConfig, &adcGroup445);
  adcChannelConfig.channelId = IfxEvadc_ChannelId_4;
  adcChannelConfig.resultRegister = IfxEvadc_ChannelResult_4; // 결과를 저장할 전용 레지스터
  IfxEvadc_Adc_initChannel(&adcChannel445[0], &adcChannelConfig);

  // Channel 1 설정
  IfxEvadc_Adc_initChannelConfig(&adcChannelConfig, &adcGroup445);
  adcChannelConfig.channelId = IfxEvadc_ChannelId_5;
  adcChannelConfig.resultRegister = IfxEvadc_ChannelResult_5;
  IfxEvadc_Adc_initChannel(&adcChannel445[1], &adcChannelConfig);

  /* 4. 변환 큐에 채널 추가 (자동 재충전 옵션 사용) */
  IfxEvadc_Adc_addToQueue(&adcChannel445[0], IfxEvadc_RequestSource_queue0, IFXEVADC_QUEUE_REFILL);
  IfxEvadc_Adc_addToQueue(&adcChannel445[1], IfxEvadc_RequestSource_queue0, IFXEVADC_QUEUE_REFILL);

  /* 5. 큐 변환 시작 */
  IfxEvadc_Adc_startQueue(&adcGroup445, IfxEvadc_RequestSource_queue0);
}

static void read_EVADC_Values301(uint16 *res1, uint16 *res2)
{
  Ifx_EVADC_G_RES conversionResult;

  /* Channel 0 결과 읽기 */
  do
  {
    // 결과 레지스터에서 값을 읽어옵니다.
    conversionResult = IfxEvadc_Adc_getResult(&adcChannel301[0]);
  } while (!conversionResult.B.VF); // Valid Flag(유효성 플래그)가 설정될 때까지 대기

  *res1 = conversionResult.B.RESULT;

  /* Channel 1 결과 읽기 */
  do
  {
    conversionResult = IfxEvadc_Adc_getResult(&adcChannel301[1]);
  } while (!conversionResult.B.VF);

  *res2 = conversionResult.B.RESULT;
}

static void read_EVADC_Values445(uint16 *res1, uint16 *res2)
{
  Ifx_EVADC_G_RES conversionResult;

  /* Channel 4 결과 읽기 */
  do
  {
    // 결과 레지스터에서 값을 읽어옵니다.
    conversionResult = IfxEvadc_Adc_getResult(&adcChannel445[0]);
  } while (!conversionResult.B.VF); // Valid Flag(유효성 플래그)가 설정될 때까지 대기

  *res1 = conversionResult.B.RESULT;

  /* Channel  결과 읽기 */
  do
  {
    conversionResult = IfxEvadc_Adc_getResult(&adcChannel445[1]);
  } while (!conversionResult.B.VF);

  *res2 = conversionResult.B.RESULT;
}

uint16 get_motor_current_adc()
{
  static uint16 resq0[10];
  static uint16 resq1[10];
  static uint8 cur0 = 0, cur1 = 0;
  read_EVADC_Values301(&adcResult0, &adcResult1);
  resq0[cur0++] = adcResult0;
  resq1[cur1++] = adcResult1;
  cur0 %= 10, cur1 %= 10;

  uint32 avg0 = 0, avg1 = 0;
  for (int i = 0; i < 10; i++) avg0 += resq0[i], avg1 += resq1[i];

  return avg0 < avg1 ? ((uint16)(avg0 / 10)) : ((uint16)(avg1 / 10));
}

#define OKZONE_LOWER_L 50
#define OKZONE_LOWER_H 200
#define OKZONE_UPPER_L 3850
#define OKZONE_UPPER_H 4050
joystick_dir_e Joystick_read(void)
{
  uint16 x, y;
  static joystick_dir_e dirx = JOY_NEUTRAL;
  static joystick_dir_e diry = JOY_NEUTRAL;

  read_EVADC_Values445(&x, &y);
  if (dirx == JOY_NEUTRAL)
  {
    if (x < OKZONE_LOWER_L) dirx = JOY_LEFT;
    else if (x > OKZONE_UPPER_H) dirx = JOY_RIGHT;
  }
  else if (dirx == JOY_LEFT)
  {
    if (x > OKZONE_LOWER_H) dirx = JOY_NEUTRAL;
  }
  else if (dirx == JOY_RIGHT)
  {
    if (x < OKZONE_UPPER_L) dirx = JOY_NEUTRAL;
  }

  if (diry == JOY_NEUTRAL)
  {
    if (y < OKZONE_LOWER_L) diry = JOY_UP;
    else if (y > OKZONE_UPPER_H) diry = JOY_DOWN;
  }
  else if (diry == JOY_UP)
  {
    if (y > OKZONE_LOWER_H) diry = JOY_NEUTRAL;
  }
  else if (diry == JOY_DOWN)
  {
    if (y < OKZONE_UPPER_L) diry = JOY_NEUTRAL;
  }

  if (dirx == JOY_NEUTRAL) return diry;
  if (diry == JOY_NEUTRAL) return dirx;
  return JOY_NEUTRAL;
}

#define DEADZONE 3
static volatile uint16 neutral_level = 0;
sint8 Joystick_read_level(void)
{
  uint16 x, y;
  sint8 yy;

  read_EVADC_Values445(&x, &y);
#define NEWADC
  if ((y > neutral_level && y - neutral_level < 500) || (y < neutral_level && neutral_level - y < 500))
    yy = 0;
  else
    yy = (float)y * 200.0f / 4096.0f - 100.0f;
#ifdef NEWADC
#else
  if (y > neutral_level)
  {
    yy = ((float)y - neutral_level) * 100.0f / (float)(4095 - neutral_level);
  }
  else
  {
    yy = (-(float)y) * 100.0f / (float)(neutral_level);
  }
  if (yy < DEADZONE && yy > -DEADZONE) yy = 0;
#endif
  return yy;
}

void setneutral(void)
{
  read_EVADC_Values445(&adcResult0, &adcResult1);
  neutral_level = adcResult1;
}