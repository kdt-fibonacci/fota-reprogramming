# 23. Application Change Log

## 관련 문서

- [Wiki Home](./README.md)
- [System Architecture](./02_system_architecture.md)
- [Runtime Main Loops](./19_runtime_main_loops.md)
- [FOTA Update Flow](./15_fota_update_flow.md)
- [Open Issues](./22_open_issues.md)

## 1. 목적

이 문서는 초기 Developer Wiki 생성 이후 app/application layer 변경사항을 추적한다. 본 회차 변경은 커밋 `8231dc7 add: App`을 기준으로 한다(`git show --stat 8231dc7`).

> 모든 항목은 현재 코드 기준이며, 직접 근거가 없는 추론은 `추정`, 모호한 부분은 `확인 필요`로 표시한다. 본 회차 변경은 **Target ECU(Motion/Lighting)**에 집중되며, Gateway/RPI Master/Linux Server의 소스 변경은 없다.

## 2. 변경 요약

| Area | Change | Evidence | Updated Docs |
|---|---|---|---|
| Target main loop | cooperative `Test_MainFunctions()`+`DelayMs(1)` → STM 1ms 인터럽트 기반 `App_Scheduler` | `core0_main()` diff in `Cpu0_Main.c`, `App_Scheduler.c`, `Driver_Stm.c` | 01, 04, 05, 19 |
| Motion application | joystick→throttle motor 제어(100ms slot) 신규 | `Apps/accel.c`, `App_Scheduler_Run_100ms()` | 04, 19 |
| Lighting application | 버튼/센서→밝기 PWM lamp 제어(10ms slot) 신규 | `Apps/App_Lamp.c`, `App_Scheduler_Run_10ms()` | 05, 19 |
| 신규 driver | STM tick, VADC, GTM PWM (Motion/Lighting) | `Basics/Common/{Driver_Stm,adc,pwm}.c` | 01, 04, 05 |
| 0x11 ECUReset | positive 응답 후 `Shared_Util_Time_DelayMs(1000)` 추가 후 reset | `Dcm_HandleEcuReset()` diff in `Dcm.c` | 14, 17, 20 |
| 디버그 기본값 | `DEBUG_FOTA_ENABLE` `1U`→`0U` (Motion/Lighting) | `Utils/Debug_Cfg.h` diff | 20 |
| 부팅 로그 | `[Motion/Lighting ECU] Current Version: A` UART 출력 추가 | `core0_main()` diff | 17, 20 |
| 기술 정정 | Motion/Lighting "바이트 단위 동일" → application 분기 | `accel.c` vs `App_Lamp.c` | 04, 05, 00 |
| 배치 명확화 | RPI Master = 차량 내부 High Performance Computer(In-Vehicle HPC) | 프로젝트 배치 기준(사용자 제공) + 네트워크 대역 분리 | 00, 02, 07, README |

## 3. 변경된 런타임 흐름

| Flow | Previous Behavior | Current Behavior | Evidence |
|---|---|---|---|
| Motion/Lighting main loop | `while(1){ Test_MainFunctions(); DelayMs(1);}` 직접 호출, 1ms cooperative | `while(1){ App_Scheduler_Run(); }` — STM compare IRQ가 1/10/100/1000ms flag set → slot 디스패치 | `App_Scheduler_Run()` in `Apps/App_Scheduler.c`; `STM_Int0Handler()` in `Basics/Common/Driver_Stm.c` |
| 통신 스택 처리 | main loop에서 Read→CanTp→Dcm→Write→FOTA 1회 | 1ms slot에서 처리하되 `Can_MainFunction_Write()`를 단계 사이 다회 호출 | `App_Scheduler_Run_1ms()` |
| Motion application | 없음 | 100ms slot: `setThrottle()`(joystick→PWM), `isStopped()` 시 `P00.5` 제어 | `App_Scheduler_Run_100ms()`, `accel.c` |
| Lighting application | 없음 | 10ms slot: `lamp10mstask()` 버튼 입력→밝기 점증/감→`PWM_setDutyCycle()` | `App_Scheduler_Run_10ms()`, `App_Lamp.c` |
| 0x11 ECUReset | 응답 송신 직후 즉시 `FOTA_PerformSystemReset()` | 응답 송신 → `Shared_Util_Time_DelayMs(1000)` → reset | `Dcm_HandleEcuReset()` diff |

## 4. 변경된 상태 / 모듈 / API

| Item | Type | Change | Evidence |
|---|---|---|---|
| `App_Scheduler_Init()` / `App_Scheduler_Run()` | function | 신규(Motion/Lighting) | `Apps/App_Scheduler.c/.h` |
| `App_Scheduler_Run_{1,10,100,1000}ms()` | static function | 신규 slot 핸들러 | `Apps/App_Scheduler.c` |
| `Driver_Stm_Init()` / `initSTM()` / `STM_Int0Handler()` | function | 신규 STM tick 소스 | `Basics/Common/Driver_Stm.c` |
| `SchedulingFlag` / `stSchedulingInfo` | struct / global | 신규 scheduling flag | `Basics/Common/Driver_Stm.h` |
| `App_stm` / `stCnt` | struct | 신규 STM/카운터 컨텍스트 | `Driver_Stm.h` |
| `motor_init()` / `setThrottle()` / `isStopped()` | function | 신규(Motion) motor 제어 | `Apps/accel.c/.h` |
| `lampinit()` / `lamp10mstask()` | function | 신규(Lighting) lamp 제어 | `Apps/App_Lamp.c/.h` |
| `Joystick_read_level()` / `init_VADC_*` / `get_motor_current_adc()` | function | 신규(Motion) VADC | `Basics/Common/adc.c/.h` |
| `read_EVADC_Values31()` / `init_VADC_Group3_Ch1()` | function | 신규(Lighting) 조도 VADC | `Basics/Common/adc.c/.h` |
| `PWM_setDutyCycle()` / `init_GTM_PWM3_TOUT1`(Motion) / `init_GTM_PWM3_TOUT104`(Lighting) | function | 신규 GTM PWM | `Basics/Common/pwm.c/.h` |
| `Test_MainFunctions()` | function | **제거**(Motion/Lighting), Gateway는 잔존 | `Cpu0_Main.c` diff |
| `Dcm.c` include | dependency | `#include "Time.h"` 추가 | `Dcm.c` diff |
| `DEBUG_FOTA_ENABLE` | macro | `1U`→`0U`(Motion/Lighting) | `Utils/Debug_Cfg.h` diff |

> Dcm 서비스 dispatch, FotaHandler, Sota* swap/flash 코어 로직은 이번 변경에서 수정되지 않았다(0x11 직전 delay 추가만 예외).

## 5. 변경된 Mermaid Diagram

| Document | Diagram | Change |
|---|---|---|
| 19_runtime_main_loops.md | Motion/Lighting main loop | cooperative loop → STM scheduler slot flow로 교체 |
| 04_motion_ecu_architecture.md | 주요 구성요소 / 9.3 main loop | App_Scheduler + 100ms throttle slot 반영 |
| 05_lighting_ecu_architecture.md | 8. main loop + 8.1 lamp 흐름 | STM scheduler + 10ms lamp slot 추가 |
| 01_repository_structure.md | ECU 공통 폴더 구조 | `Apps/`, `Basics/Common` STM/ADC/PWM 추가 |
| 14_dcm_diagnostic_services.md | RoutineControl/reset 시퀀스 | 0x51 후 1000ms delay 단계 추가 |
| 17_reset_boot_bank_swap.md | activation→reset 시퀀스 | 1000ms delay 단계 추가 |
| 20_debugging_notes.md | reset 디버깅 흐름 | 1000ms delay + Current Version 로그 반영 |
| 02_system_architecture.md / 00 / 07 | 시스템 context / 네트워크 | 차량 외부 백엔드 ↔ 차량 내부 HPC 경계 명시 |

## 6. 문서 반영 내역

| Document | Update |
|---|---|
| 00_project_overview.md | 노드 표/개념도에 In-Vehicle HPC + Motion/Lighting application 반영, 최근 변경 섹션 |
| 01_repository_structure.md | `Apps/`·`Basics/Common` 신규 파일, `Test_MainFunctions` 제거 반영 |
| 02_system_architecture.md | 차량 외부/내부 경계, HPC 포지셔닝 |
| 04_motion_ecu_architecture.md | App_Scheduler/motor application, Lighting과 차이 정정 |
| 05_lighting_ecu_architecture.md | lamp application, "Motion 동일" 정정 |
| 07_rpi_fota_master_architecture.md | In-Vehicle HPC 배치 포지셔닝 |
| 14_dcm_diagnostic_services.md | 0x11 1000ms delay |
| 17_reset_boot_bank_swap.md | reset 직전 delay, 버전 로그 |
| 19_runtime_main_loops.md | Target ECU STM scheduler 전환 |
| 20_debugging_notes.md | DEBUG_FOTA_ENABLE=0, reset delay, STM 의존성 |
| 23_application_change_log.md | 본 문서 신규 |
| README.md | 23 링크 + 읽기 순서 갱신 |

## 7. 확인 필요 사항

| Item | Reason |
|---|---|
| `Shared_Util_Time_DelayMs(1000)`의 응답 송출 보장 여부 | blocking delay 구현/CanTp Tx 완료 시점 의존 |
| 1ms slot 처리 지연이 10/100/1000ms slot 타이밍에 주는 영향 | flag는 1ms tick마다 set, slot은 1ms slot 실행 시 검사 |
| `App_Scheduler_Run_1ms()`가 `Can_MainFunction_Write()`를 4회 호출하는 의도 | Tx latency 최소화 `추정` |
| RPI Master의 차량 내부 HPC 물리 토폴로지/회선 | 배치는 사용자 제공, 물리 구성 미확인 |
| Motion `accel.c`의 `TCS_ENABLE` 등 매크로 운영 의도 | 코드상 `#define TCS_ENABLE`가 함수 내부에 위치 |
| Lighting `AUTOMODE`(조도 자동 점등) 비활성 사유 | `//#define AUTOMODE` 주석 처리, 현재 버튼 모드 |

## 다음에 읽을 문서

- [Runtime Main Loops](./19_runtime_main_loops.md)
- [FOTA Update Flow](./15_fota_update_flow.md)
- [Debugging Notes](./20_debugging_notes.md)
- [Open Issues](./22_open_issues.md)
