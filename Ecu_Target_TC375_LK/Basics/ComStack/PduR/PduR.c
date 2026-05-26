/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "PduR.h"
#include "PduR_Cfg.h"

#include "Dcm.h"
#include "CanTp.h"
#include "DoIP.h"

#include "Debug_Log.h"

/*********************************************************************************************************************/
/*------------------------------------------------Static Variables---------------------------------------------------*/
/*********************************************************************************************************************/

static boolean PduR_Initialized = FALSE;

/*********************************************************************************************************************/
/*------------------------------------------------Private Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static const PduR_RoutingPathConfigType* PduR_FindRoutingPath(
    PduR_ModuleType SourceModule,
    PduIdType SourcePduId,
    PduR_EventType RoutingEvent
);

static Std_ReturnType PduR_RouteTransmit(
    const PduR_RoutingPathConfigType* RouteConfig,
    const PduInfoType* PduInfoPtr
);

static void PduR_RouteRxIndication(
    const PduR_RoutingPathConfigType* RouteConfig,
    const PduInfoType* PduInfoPtr
);

static void PduR_RouteTxConfirmation(
    const PduR_RoutingPathConfigType* RouteConfig,
    Std_ReturnType Result
);

/*********************************************************************************************************************/
/*------------------------------------------------Public Functions---------------------------------------------------*/
/*********************************************************************************************************************/

void PduR_Init(void)
{
    PduR_Initialized = TRUE;
}

Std_ReturnType PduR_DcmTransmit(
    PduIdType DcmTxPduId,
    const PduInfoType* PduInfoPtr
)
{
    const PduR_RoutingPathConfigType* RouteConfig;

    if (PduR_Initialized == FALSE)
    {
        return E_NOT_OK;
    }

    if (PduInfoPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    RouteConfig = PduR_FindRoutingPath(
        PDUR_MODULE_DCM,
        DcmTxPduId,
        PDUR_EVENT_TRANSMIT
    );

    if (RouteConfig == NULL_PTR)
    {
        return E_NOT_OK;
    }

    PDUR_DEBUG_PRINTF(
        "[PduR][TX] Src=Dcm SrcPduId=%u Route=%u DestModule=%u DestPduId=%u",
        (unsigned int)DcmTxPduId,
        (unsigned int)RouteConfig->PduRRoutingPathId,
        (unsigned int)RouteConfig->DestModule,
        (unsigned int)RouteConfig->DestPduId
    );
    PDUR_DEBUG_PRINT_PDU("", PduInfoPtr);

    return PduR_RouteTransmit(
        RouteConfig,
        PduInfoPtr
    );
}

void PduR_DoIPTpRxIndication(
    PduIdType DoIPRxPduId,
    const PduInfoType* PduInfoPtr
)
{
    const PduR_RoutingPathConfigType* RouteConfig;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    if (PduInfoPtr == NULL_PTR)
    {
        return;
    }

    RouteConfig = PduR_FindRoutingPath(
        PDUR_MODULE_DOIPTP,
        DoIPRxPduId,
        PDUR_EVENT_RX_INDICATION
    );

    if (RouteConfig == NULL_PTR)
    {
        return;
    }

    PDUR_DEBUG_PRINTF(
        "[PduR][RX] Src=DoIP SrcPduId=%u Route=%u DestModule=%u DestPduId=%u",
        (unsigned int)DoIPRxPduId,
        (unsigned int)RouteConfig->PduRRoutingPathId,
        (unsigned int)RouteConfig->DestModule,
        (unsigned int)RouteConfig->DestPduId
    );
    PDUR_DEBUG_PRINT_PDU("", PduInfoPtr);

    PduR_RouteRxIndication(
        RouteConfig,
        PduInfoPtr
    );
}

void PduR_DoIPTpTxConfirmation(
    PduIdType DoIPTxPduId,
    Std_ReturnType Result
)
{
    const PduR_RoutingPathConfigType* RouteConfig;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    RouteConfig = PduR_FindRoutingPath(
        PDUR_MODULE_DOIPTP,
        DoIPTxPduId,
        PDUR_EVENT_TX_CONFIRMATION
    );

    if (RouteConfig == NULL_PTR)
    {
        return;
    }

    PDUR_DEBUG_PRINTF(
        "[PduR][TX-CNF] Src=DoIP SrcPduId=%u Route=%u DestModule=%u DestPduId=%u Result=%u\r\n",
        (unsigned int)DoIPTxPduId,
        (unsigned int)RouteConfig->PduRRoutingPathId,
        (unsigned int)RouteConfig->DestModule,
        (unsigned int)RouteConfig->DestPduId,
        (unsigned int)Result
    );

    PduR_RouteTxConfirmation(
        RouteConfig,
        Result
    );
}

void PduR_CanTpRxIndication(
    PduIdType PduRRxPduId,
    const PduInfoType* PduInfoPtr
)
{
    const PduR_RoutingPathConfigType* RouteConfig;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    if (PduInfoPtr == NULL_PTR)
    {
        return;
    }

    RouteConfig = PduR_FindRoutingPath(
        PDUR_MODULE_CANTP,
        PduRRxPduId,
        PDUR_EVENT_RX_INDICATION
    );

    if (RouteConfig == NULL_PTR)
    {
        return;
    }

    PDUR_DEBUG_PRINTF(
        "[PduR][RX] Src=CanTp SrcPduId=%u Route=%u DestModule=%u DestPduId=%u",
        (unsigned int)PduRRxPduId,
        (unsigned int)RouteConfig->PduRRoutingPathId,
        (unsigned int)RouteConfig->DestModule,
        (unsigned int)RouteConfig->DestPduId
    );
    PDUR_DEBUG_PRINT_PDU("", PduInfoPtr);

    PduR_RouteRxIndication(
        RouteConfig,
        PduInfoPtr
    );
}

void PduR_CanTpTxConfirmation(
    PduIdType PduRTxPduId,
    Std_ReturnType Result
)
{
    const PduR_RoutingPathConfigType* RouteConfig;

    if (PduR_Initialized == FALSE)
    {
        return;
    }

    RouteConfig = PduR_FindRoutingPath(
        PDUR_MODULE_CANTP,
        PduRTxPduId,
        PDUR_EVENT_TX_CONFIRMATION
    );

    if (RouteConfig == NULL_PTR)
    {
        return;
    }

    PDUR_DEBUG_PRINTF(
        "[PduR][TX-CNF] Src=CanTp SrcPduId=%u Route=%u DestModule=%u DestPduId=%u Result=%u\r\n",
        (unsigned int)PduRTxPduId,
        (unsigned int)RouteConfig->PduRRoutingPathId,
        (unsigned int)RouteConfig->DestModule,
        (unsigned int)RouteConfig->DestPduId,
        (unsigned int)Result
    );

    PduR_RouteTxConfirmation(
        RouteConfig,
        Result
    );
}

/*********************************************************************************************************************/
/*------------------------------------------------Private Functions--------------------------------------------------*/
/*********************************************************************************************************************/

static const PduR_RoutingPathConfigType* PduR_FindRoutingPath(
    PduR_ModuleType SourceModule,
    PduIdType SourcePduId,
    PduR_EventType RoutingEvent
)
{
    uint8 Index;

    for (Index = 0U; Index < PDUR_ROUTING_PATH_COUNT; Index++)
    {
        if ((PduR_RoutingPathConfig[Index].SourceModule == SourceModule) &&
            (PduR_RoutingPathConfig[Index].SourcePduId == SourcePduId) &&
            (PduR_RoutingPathConfig[Index].RoutingEvent == RoutingEvent))
        {
            return &PduR_RoutingPathConfig[Index];
        }
    }

    return NULL_PTR;
}

static Std_ReturnType PduR_RouteTransmit(
    const PduR_RoutingPathConfigType* RouteConfig,
    const PduInfoType* PduInfoPtr
)
{
    Std_ReturnType Result;

    Result = E_NOT_OK;

    if ((RouteConfig == NULL_PTR) || (PduInfoPtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    switch (RouteConfig->DestModule)
    {
        case PDUR_MODULE_CANTP:
        {
            Result = CanTp_Transmit(
                RouteConfig->DestPduId,
                PduInfoPtr
            );
            break;
        }

        case PDUR_MODULE_DOIPTP:
        {
            Result = DoIP_TpTransmit(
                RouteConfig->DestPduId,
                PduInfoPtr
            );
            break;
        }

        case PDUR_MODULE_DCM:
        default:
        {
            Result = E_NOT_OK;
            break;
        }
    }

    return Result;
}

static void PduR_RouteRxIndication(
    const PduR_RoutingPathConfigType* RouteConfig,
    const PduInfoType* PduInfoPtr
)
{
    if ((RouteConfig == NULL_PTR) || (PduInfoPtr == NULL_PTR))
    {
        return;
    }

    switch (RouteConfig->DestModule)
    {
        case PDUR_MODULE_DCM:
        {
            Dcm_RxIndication(
                RouteConfig->DestPduId,
                PduInfoPtr
            );
            break;
        }

        case PDUR_MODULE_CANTP:
        {
            (void)CanTp_Transmit(
                RouteConfig->DestPduId,
                PduInfoPtr
            );
            break;
        }

        case PDUR_MODULE_DOIPTP:
        {
            (void)DoIP_TpTransmit(
                RouteConfig->DestPduId,
                PduInfoPtr
            );
            break;
        }

        default:
        {
            break;
        }
    }
}

static void PduR_RouteTxConfirmation(
    const PduR_RoutingPathConfigType* RouteConfig,
    Std_ReturnType Result
)
{
    if (RouteConfig == NULL_PTR)
    {
        return;
    }

    switch (RouteConfig->DestModule)
    {
        case PDUR_MODULE_DCM:
        {
            Dcm_TxConfirmation(
                RouteConfig->DestPduId,
                Result
            );
            break;
        }

        case PDUR_MODULE_CANTP:
        case PDUR_MODULE_DOIPTP:
        default:
        {
            break;
        }
    }
}
