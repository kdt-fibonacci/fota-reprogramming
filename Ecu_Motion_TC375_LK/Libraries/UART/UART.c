#include "IfxAsclin_Asc.h"
#include "IfxCpu_Irq.h"
#include "IfxPort.h"
#include <stdarg.h>
#include <stdio.h>
#include <UART.h>

#define ISR_PRIORITY_ASCLIN0_TX  20
#define ISR_PRIORITY_ASCLIN0_RX  21
#define ISR_PRIORITY_ASCLIN0_ER  22

IfxAsclin_Asc g_asc;

/* iLLD FIFO 버퍼 */
static uint8 g_txBuffer[UART_TX_BUFFER_SIZE + sizeof(Ifx_Fifo) + 8];
static uint8 g_rxBuffer[UART_RX_BUFFER_SIZE + sizeof(Ifx_Fifo) + 8];

/* ISR */
IFX_INTERRUPT(asclin0TxISR, 0, ISR_PRIORITY_ASCLIN0_TX)
{
    IfxAsclin_Asc_isrTransmit(&g_asc);
}

IFX_INTERRUPT(asclin0RxISR, 0, ISR_PRIORITY_ASCLIN0_RX)
{
    IfxAsclin_Asc_isrReceive(&g_asc);
}

IFX_INTERRUPT(asclin0ErISR, 0, ISR_PRIORITY_ASCLIN0_ER)
{
    IfxAsclin_Asc_isrError(&g_asc);
}

void UART_Init(void)
{
    IfxAsclin_Asc_Config ascConfig;
    IfxAsclin_Asc_initModuleConfig(&ascConfig, &MODULE_ASCLIN0);

    /* baudrate */
    ascConfig.baudrate.baudrate = 115200U;
    ascConfig.baudrate.oversampling = IfxAsclin_OversamplingFactor_16;

    /* interrupt */
    ascConfig.interrupt.txPriority = ISR_PRIORITY_ASCLIN0_TX;
    ascConfig.interrupt.rxPriority = ISR_PRIORITY_ASCLIN0_RX;
    ascConfig.interrupt.erPriority = ISR_PRIORITY_ASCLIN0_ER;
    ascConfig.interrupt.typeOfService = IfxSrc_Tos_cpu0;

    /* FIFO buffer */
    ascConfig.txBuffer = g_txBuffer;
    ascConfig.txBufferSize = UART_TX_BUFFER_SIZE;
    ascConfig.rxBuffer = g_rxBuffer;
    ascConfig.rxBufferSize = UART_RX_BUFFER_SIZE;

    /* TC375 Lite V2: USB VCOM = ASCLIN0 on P14.0/P14.1 */
    static const IfxAsclin_Asc_Pins pins = {
        NULL_PTR,                  IfxPort_InputMode_pullUp,     /* CTS */
        &IfxAsclin0_RXA_P14_1_IN,  IfxPort_InputMode_pullUp,     /* RX  */
        NULL_PTR,                  IfxPort_OutputMode_pushPull,  /* RTS */
        &IfxAsclin0_TX_P14_0_OUT,  IfxPort_OutputMode_pushPull,  /* TX  */
        IfxPort_PadDriver_cmosAutomotiveSpeed1
    };
    ascConfig.pins = &pins;

    IfxAsclin_Asc_initModule(&g_asc, &ascConfig);
}

void UART_SendByte(uint8 data)
{
    IfxAsclin_Asc_blockingWrite(&g_asc, data);
}

void UART_Printf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len < 0)
        return;

    if (len >= (int)sizeof(buf))
        len = sizeof(buf) - 1;

    for (int i = 0; i < len; ++i)
    {
        UART_SendByte((uint8)buf[i]);
    }
}
