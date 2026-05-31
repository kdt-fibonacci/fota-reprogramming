#include "Gtm/Pwm/IfxGtm_Pwm.h"
#include "Gtm/Tom/Pwm/IfxGtm_Tom_Pwm.h"
#include "IfxGtm_PinMap.h"

/* 전역 드라이버 구조체 선언 */
IfxGtm_Tom_Pwm_Driver g_tomPwmDriver_TOUT104;

/* PWM 기본 설정값 (단위: 틱) */
#define PWM_PERIOD     5000 /* PWM 주기 (주파수 결정) */
#define PWM_DUTY_CYCLE 0    /* 초기 듀티 사이클 (0%) */

/*
 * GTM TOM을 이용해 TOUT1 PWM 출력을 초기화하는 API
 */
void init_GTM_PWM3_TOUT104(void)
{
  /* 1. GTM 모듈 활성화 */
  Ifx_GTM *gtm = &MODULE_GTM;
  IfxGtm_enable(gtm);

  /* 2. CMU(Clock Management Unit) 클럭 활성화 (FXCLK 사용) */
  IfxGtm_Cmu_enableClocks(gtm, IFXGTM_CMU_CLKEN_FXCLK);

  /* 3. TOM PWM 기본 설정 구조체 초기화 */
  IfxGtm_Tom_Pwm_Config tomConfig1;
  IfxGtm_Tom_Pwm_initConfig(&tomConfig1, gtm);

  /* 4. TOUT1 핀 매핑 및 하드웨어 설정
   * 주의: 사용하는 보드(AppKit, ShieldBuddy 등)에 따라 TOUT1이 연결된 포트가 다를 수 있습니다.
   * 아래는 Port 02, Pin 1 (P02.1)을 TOUT1으로 사용하는 예시입니다.
   */
  tomConfig1.tom = IfxGtm_Tom_0;                               /* TOM0 사용 */
  tomConfig1.tomChannel = IfxGtm_Tom_Ch_2;                     /* 채널 1 사용 */
  tomConfig1.pin.outputPin = &IfxGtm_TOM0_2_TOUT104_P10_2_OUT; /* TOUT1 (P02.1) 핀 매핑 */
  tomConfig1.pin.outputMode = IfxPort_OutputMode_pushPull;
  tomConfig1.pin.padDriver = IfxPort_PadDriver_cmosAutomotiveSpeed1;

  /* 5. 클럭, 주기 및 듀티 사이클 설정 */
  tomConfig1.clock = IfxGtm_Tom_Ch_ClkSrc_cmuFxclk0; /* FXCLK0 클럭 소스 선택 */
  tomConfig1.period = PWM_PERIOD;                    /* 전체 주기 설정 (SR0 레지스터) */
  tomConfig1.dutyCycle = PWM_DUTY_CYCLE;             /* High 유지 시간 설정 (SR1 레지스터) */
  tomConfig1.signalLevel = Ifx_ActiveState_high;     /* High Active 모드 */
  tomConfig1.synchronousUpdateEnabled = TRUE;        /* 동기적 업데이트 활성화 */

  /* 6. 드라이버 초기화 및 PWM 출력 시작 */
  IfxGtm_Tom_Pwm_init(&g_tomPwmDriver_TOUT104, &tomConfig1);
  IfxGtm_Tom_Pwm_start(&g_tomPwmDriver_TOUT104, TRUE);
}

/*
 * PWM의 듀티 사이클(Duty Cycle)을 실시간으로 변경하는 API
 * @param dutyCycleTicks 변경할 듀티 사이클의 틱(Tick) 값 (0-50000)
 */
static void set_GTM_PWM3_DutyCycle(uint16 dutyCycleTicks)
{
  /*
   * 글리치(Glitch) 현상을 방지하기 위해
   * Shadow Register (SR1)에 값을 기록하여 다음 주기에 반영되도록 합니다.
   */
  IfxGtm_Tom_Ch_setCompareOneShadow(g_tomPwmDriver_TOUT104.tom, g_tomPwmDriver_TOUT104.tomChannel, dutyCycleTicks);
  // IfxGtm_Tom_Ch_setCompareOne(g_tomPwmDriver_TOUT104.tom, g_tomPwmDriver_TOUT104.tomChannel, dutyCycleTicks);
}

void PWM_setDutyCycle(uint16 brightness)
{
  set_GTM_PWM3_DutyCycle(brightness);
}
