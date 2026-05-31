#include "IfxStm.h"
#include "IfxCpu_Irq.h"
#include "Bsp.h"

typedef struct {
        Ifx_STM                 *stmSfr;
        IfxStm_CompareConfig    stmConfig;
        volatile uint8          LedBlink;
        volatile unsigned int   counter;
} App_stm;

typedef struct
{
        volatile uint8 u8nuScheduling1msFlag;
        volatile uint8 u8nuScheduling10msFlag;
        volatile uint8 u8nuScheduling100msFlag;
        volatile uint8 u8nuScheduling1000msFlag;
} SchedulingFlag;

typedef struct
{
        volatile unsigned int u32nuCnt1ms;
        volatile unsigned int u32nuCnt10ms;
        volatile unsigned int u32nuCnt100ms;
        volatile unsigned int u32nuCnt1000ms;
} stCnt;

extern volatile SchedulingFlag stSchedulingInfo;

void initSTM(void);
void Driver_Stm_Init(void);
