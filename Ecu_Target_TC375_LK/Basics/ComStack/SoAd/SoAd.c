/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "lwip/mem.h"
#include "lwip/debug.h"

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"

#include "SoAd.h"
#include "SoAd_Cfg.h"

#include "DoIP.h"
#include "Debug_Log.h"

#include <string.h>

/*********************************************************************************************************************/
/*------------------------------------------------------Types--------------------------------------------------------*/
/*********************************************************************************************************************/

typedef enum
{
    SOAD_SOCON_STATE_OFFLINE = 0U,
    SOAD_SOCON_STATE_LISTENING,
    SOAD_SOCON_STATE_ONLINE,
    SOAD_SOCON_STATE_CLOSING
} SoAd_SoConStateType;

typedef struct
{
    boolean IsUsed;

    SoAd_SoConStateType State;

    uint8 SoConId;
    uint8 Retries;

    struct tcp_pcb* ListenPcb;
    struct tcp_pcb* ConnectionPcb;

    uint8 RxBuffer[SOAD_RX_BUFFER_SIZE];
    uint16 RxLength;

    uint8 TxBuffer[SOAD_TX_BUFFER_SIZE];
} SoAd_SocketConnectionRuntimeType;

/*********************************************************************************************************************/
/*------------------------------------------------Static Variables---------------------------------------------------*/
/*********************************************************************************************************************/

static SoAd_SocketConnectionRuntimeType SoAd_SocketConnectionRuntime[SOAD_SOCKET_CONNECTION_COUNT];

/*********************************************************************************************************************/
/*------------------------------------------------Private Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static SoAd_SocketConnectionRuntimeType* SoAd_GetRuntimeBySoConId(
    uint8 SoConId
);

static SoAd_SocketConnectionRuntimeType* SoAd_GetRuntimeByTxPduId(
    PduIdType TxPduId
);

static const SoAd_SocketConnectionConfigType* SoAd_GetConfigBySoConId(
    uint8 SoConId
);

static const SoAd_SocketConnectionConfigType* SoAd_GetConfigByRxPduId(
    PduIdType RxPduId
);

static const SoAd_SocketConnectionConfigType* SoAd_GetConfigByTxPduId(
    PduIdType TxPduId
);

static err_t SoAd_Accept(
    void* Arg,
    struct tcp_pcb* NewPcb,
    err_t Error
);

static err_t SoAd_Recv(
    void* Arg,
    struct tcp_pcb* TcpPcb,
    struct pbuf* Pbuf,
    err_t Error
);

static void SoAd_Error(
    void* Arg,
    err_t Error
);

static err_t SoAd_Poll(
    void* Arg,
    struct tcp_pcb* TcpPcb
);

static err_t SoAd_Sent(
    void* Arg,
    struct tcp_pcb* TcpPcb,
    u16_t Length
);

static void SoAd_Close(
    SoAd_SocketConnectionRuntimeType* Runtime
);

/*********************************************************************************************************************/
/*------------------------------------------------Public Functions---------------------------------------------------*/
/*********************************************************************************************************************/

void SoAd_Init(void)
{
    uint8 Index;
    err_t Error;
    SoAd_SocketConnectionRuntimeType* Runtime;

    memset(
        SoAd_SocketConnectionRuntime,
        0x00,
        sizeof(SoAd_SocketConnectionRuntime)
    );

    for (Index = 0U; Index < SOAD_SOCKET_CONNECTION_COUNT; Index++)
    {
        Runtime = &SoAd_SocketConnectionRuntime[Index];

        Runtime->IsUsed = TRUE;
        Runtime->SoConId = SoAd_SocketConnectionConfig[Index].SoConId;
        Runtime->State = SOAD_SOCON_STATE_OFFLINE;
        Runtime->RxLength = 0U;

        Runtime->ListenPcb = tcp_new();

        if (Runtime->ListenPcb == NULL_PTR)
        {
            continue;
        }

        Error = tcp_bind(
            Runtime->ListenPcb,
            IP_ADDR_ANY,
            SoAd_SocketConnectionConfig[Index].LocalPort
        );

        if (Error != ERR_OK)
        {
            tcp_close(Runtime->ListenPcb);
            Runtime->ListenPcb = NULL_PTR;
            continue;
        }

        Runtime->ListenPcb = tcp_listen(Runtime->ListenPcb);

        if (Runtime->ListenPcb == NULL_PTR)
        {
            continue;
        }

        tcp_arg(
            Runtime->ListenPcb,
            Runtime
        );

        tcp_accept(
            Runtime->ListenPcb,
            SoAd_Accept
        );

        Runtime->State = SOAD_SOCON_STATE_LISTENING;
    }
}

Std_ReturnType SoAd_Transmit(
    PduIdType SoAdTxPduId,
    const PduInfoType* PduInfoPtr
)
{
    SoAd_SocketConnectionRuntimeType* Runtime;
    err_t Error;

    Runtime = SoAd_GetRuntimeByTxPduId(SoAdTxPduId);

    if (Runtime == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (Runtime->ConnectionPcb == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (Runtime->State != SOAD_SOCON_STATE_ONLINE)
    {
        return E_NOT_OK;
    }

    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (PduInfoPtr->SduLength > SOAD_TX_BUFFER_SIZE)
    {
        return E_NOT_OK;
    }

    if (tcp_sndbuf(Runtime->ConnectionPcb) < PduInfoPtr->SduLength)
    {
        return E_NOT_OK;
    }

    SOAD_DEBUG_PRINTF(
        "[SoAd][TX] SoAdTxPduId=%u SoConId=%u",
        (unsigned int)SoAdTxPduId,
        (unsigned int)Runtime->SoConId
    );
    SOAD_DEBUG_PRINT_PDU("", PduInfoPtr);

    memcpy(
        Runtime->TxBuffer,
        PduInfoPtr->SduDataPtr,
        PduInfoPtr->SduLength
    );

    Error = tcp_write(
        Runtime->ConnectionPcb,
        Runtime->TxBuffer,
        PduInfoPtr->SduLength,
        TCP_WRITE_FLAG_COPY
    );

    if (Error != ERR_OK)
    {
        return E_NOT_OK;
    }

    Error = tcp_output(Runtime->ConnectionPcb);

    if (Error != ERR_OK)
    {
        return E_NOT_OK;
    }

    return E_OK;
}

/*********************************************************************************************************************/
/*------------------------------------------------TCP Callback Functions---------------------------------------------*/
/*********************************************************************************************************************/

static err_t SoAd_Accept(
    void* Arg,
    struct tcp_pcb* NewPcb,
    err_t Error
)
{
    SoAd_SocketConnectionRuntimeType* Runtime;

    if (Error != ERR_OK)
    {
        return Error;
    }

    Runtime = (SoAd_SocketConnectionRuntimeType*)Arg;

    if (Runtime == NULL_PTR)
    {
        tcp_abort(NewPcb);
        return ERR_ABRT;
    }

    Runtime->ConnectionPcb = NewPcb;
    Runtime->Retries = 0U;
    Runtime->RxLength = 0U;
    Runtime->State = SOAD_SOCON_STATE_ONLINE;

    tcp_setprio(
        NewPcb,
        TCP_PRIO_MAX
    );

    tcp_arg(
        NewPcb,
        Runtime
    );

    tcp_recv(
        NewPcb,
        SoAd_Recv
    );

    tcp_err(
        NewPcb,
        SoAd_Error
    );

    tcp_sent(
        NewPcb,
        SoAd_Sent
    );

    tcp_poll(
        NewPcb,
        SoAd_Poll,
        4U
    );

    return ERR_OK;
}

static err_t SoAd_Recv(
    void* Arg,
    struct tcp_pcb* TcpPcb,
    struct pbuf* Pbuf,
    err_t Error
)
{
    SoAd_SocketConnectionRuntimeType* Runtime;
    const SoAd_SocketConnectionConfigType* Config;
    struct pbuf* CurrentPbuf;
    uint16 CopyOffset;
    uint16 CopyLength;
    PduInfoType PduInfo;

    if (Error != ERR_OK)
    {
        return Error;
    }

    Runtime = (SoAd_SocketConnectionRuntimeType*)Arg;

    if (Runtime == NULL_PTR)
    {
        if (Pbuf != NULL_PTR)
        {
            pbuf_free(Pbuf);
        }

        return ERR_ARG;
    }

    if (Pbuf == NULL_PTR)
    {
        Runtime->State = SOAD_SOCON_STATE_CLOSING;
        SoAd_Close(Runtime);
        return ERR_OK;
    }

    if (Pbuf->tot_len > SOAD_RX_BUFFER_SIZE)
    {
        tcp_recved(
            TcpPcb,
            Pbuf->tot_len
        );

        pbuf_free(Pbuf);
        return ERR_OK;
    }

    CopyOffset = 0U;
    CurrentPbuf = Pbuf;

    while (CurrentPbuf != NULL_PTR)
    {
        CopyLength = CurrentPbuf->len;

        memcpy(
            &Runtime->RxBuffer[CopyOffset],
            CurrentPbuf->payload,
            CopyLength
        );

        CopyOffset = (uint16)(CopyOffset + CopyLength);
        CurrentPbuf = CurrentPbuf->next;
    }

    Runtime->RxLength = Pbuf->tot_len;

    tcp_recved(
        TcpPcb,
        Pbuf->tot_len
    );

    pbuf_free(Pbuf);

    Config = SoAd_GetConfigBySoConId(Runtime->SoConId);

    if (Config == NULL_PTR)
    {
        return ERR_OK;
    }

    PduInfo.SduDataPtr = Runtime->RxBuffer;
    PduInfo.SduLength  = Runtime->RxLength;

    SOAD_DEBUG_PRINTF(
        "[SoAd][RX] SoConId=%u RxPduId=%u",
        (unsigned int)Runtime->SoConId,
        (unsigned int)Config->RxPduId
    );
    SOAD_DEBUG_PRINT_PDU("", &PduInfo);

    DoIP_TpRxIndication(
        Config->RxPduId,
        &PduInfo
    );

    return ERR_OK;
}

static void SoAd_Error(
    void* Arg,
    err_t Error
)
{
    SoAd_SocketConnectionRuntimeType* Runtime;

    (void)Error;

    Runtime = (SoAd_SocketConnectionRuntimeType*)Arg;

    if (Runtime != NULL_PTR)
    {
        Runtime->ConnectionPcb = NULL_PTR;
        Runtime->RxLength = 0U;

        if (Runtime->ListenPcb != NULL_PTR)
        {
            Runtime->State = SOAD_SOCON_STATE_LISTENING;
        }
        else
        {
            Runtime->State = SOAD_SOCON_STATE_OFFLINE;
        }
    }
}

static err_t SoAd_Poll(
    void* Arg,
    struct tcp_pcb* TcpPcb
)
{
    SoAd_SocketConnectionRuntimeType* Runtime;

    Runtime = (SoAd_SocketConnectionRuntimeType*)Arg;

    if (Runtime == NULL_PTR)
    {
        tcp_abort(TcpPcb);
        return ERR_ABRT;
    }

    return ERR_OK;
}

static err_t SoAd_Sent(
    void* Arg,
    struct tcp_pcb* TcpPcb,
    u16_t Length
)
{
    SoAd_SocketConnectionRuntimeType* Runtime;

    (void)TcpPcb;

    Runtime = (SoAd_SocketConnectionRuntimeType*)Arg;

    if (Runtime != NULL_PTR)
    {
        Runtime->Retries = 0U;

        SOAD_DEBUG_PRINTF(
            "[SoAd][TX-CNF] SoConId=%u Length=%u\r\n",
            (unsigned int)Runtime->SoConId,
            (unsigned int)Length
        );
    }

    return ERR_OK;
}

/*********************************************************************************************************************/
/*------------------------------------------------Private Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static void SoAd_Close(
    SoAd_SocketConnectionRuntimeType* Runtime
)
{
    if (Runtime == NULL_PTR)
    {
        return;
    }

    if (Runtime->ConnectionPcb != NULL_PTR)
    {
        tcp_arg(Runtime->ConnectionPcb, NULL_PTR);
        tcp_recv(Runtime->ConnectionPcb, NULL_PTR);
        tcp_err(Runtime->ConnectionPcb, NULL_PTR);
        tcp_sent(Runtime->ConnectionPcb, NULL_PTR);
        tcp_poll(Runtime->ConnectionPcb, NULL_PTR, 0U);

        (void)tcp_close(Runtime->ConnectionPcb);

        Runtime->ConnectionPcb = NULL_PTR;
    }

    Runtime->RxLength = 0U;

    if (Runtime->ListenPcb != NULL_PTR)
    {
        Runtime->State = SOAD_SOCON_STATE_LISTENING;
    }
    else
    {
        Runtime->State = SOAD_SOCON_STATE_OFFLINE;
    }
}

static SoAd_SocketConnectionRuntimeType* SoAd_GetRuntimeByTxPduId(
    PduIdType TxPduId
)
{
    uint8 Index;

    for (Index = 0U; Index < SOAD_SOCKET_CONNECTION_COUNT; Index++)
    {
        if (SoAd_SocketConnectionConfig[Index].TxPduId == TxPduId)
        {
            return &SoAd_SocketConnectionRuntime[Index];
        }
    }

    return NULL_PTR;
}

static const SoAd_SocketConnectionConfigType* SoAd_GetConfigBySoConId(
    uint8 SoConId
)
{
    uint8 Index;

    for (Index = 0U; Index < SOAD_SOCKET_CONNECTION_COUNT; Index++)
    {
        if (SoAd_SocketConnectionConfig[Index].SoConId == SoConId)
        {
            return &SoAd_SocketConnectionConfig[Index];
        }
    }

    return NULL_PTR;
}
