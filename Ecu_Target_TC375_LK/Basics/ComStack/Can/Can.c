/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "Can.h"
#include "Can_Cfg.h"
#include "CanIf.h"

#include "Debug_Log.h"

/*********************************************************************************************************************/
/*---------------------------------------------Private Type Definitions----------------------------------------------*/
/*********************************************************************************************************************/

typedef struct
{
    boolean   IsPending;
    PduIdType SwPduHandle;
    uint8     CanControllerId;
    uint8     CanTxBufferIndex;
} Can_TxPendingType;

typedef struct
{
    IfxCan_Can      CanModule;
    IfxCan_Can_Node Node[CAN_CONTROLLER_COUNT];
} Can_DriverRuntimeType;

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/

static Can_ControllerStateType Can_ControllerState[CAN_CONTROLLER_COUNT];
Can_DriverRuntimeType   Can_Runtime;

/*
 * Tx pending state is indexed by HOH ID.
 * Only TRANSMIT HOHs use this table, but CAN_HOH_COUNT keeps the indexing simple.
 */
static Can_TxPendingType Can_TxPending[CAN_HOH_COUNT];

static const IfxCan_Can_Pins Can_Pins =
{
    .txPin     = &CAN_TX_PIN,
    .txPinMode = IfxPort_OutputMode_pushPull,
    .rxPin     = &CAN_RX_PIN,
    .rxPinMode = IfxPort_InputMode_pullUp,
    .padDriver = IfxPort_PadDriver_cmosAutomotiveSpeed1
};

/*********************************************************************************************************************/
/*----------------------------------------------Private Declarations-------------------------------------------------*/
/*********************************************************************************************************************/

static IfxCan_DataLengthCode Can_ConvertLengthToDlc(
    uint8 Length
);

static uint8 Can_ConvertDlcToLength(
    IfxCan_DataLengthCode DataLengthCode
);

static void Can_InitRxFilter(
    const Can_HardwareObjectConfigType* HohConfig
);

static Std_ReturnType Can_ReadRxFifo0(
    Can_HwType* Mailbox,
    PduInfoType* PduInfoPtr
);

static Std_ReturnType Can_FindRxHohByCanId(
    Can_IdType CanId,
    Can_HwHandleType* HrhPtr
);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void Can_Init(
    void
)
{
    uint8 i;
    IfxCan_Can_Config     canModuleConfig;
    IfxCan_Can_NodeConfig canNodeConfig;

    for (i = 0U; i < CAN_CONTROLLER_COUNT; i++)
    {
        Can_ControllerState[i] = CAN_CS_STOPPED;
    }

    for (i = 0U; i < CAN_HOH_COUNT; i++)
    {
        Can_TxPending[i].IsPending        = FALSE;
        Can_TxPending[i].SwPduHandle      = 0U;
        Can_TxPending[i].CanControllerId  = 0U;
        Can_TxPending[i].CanTxBufferIndex = 0U;
    }

    /* 1. CAN transceiver wakeup. */
    IfxPort_setPinModeOutput(&MODULE_P20, 6, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    IfxPort_setPinLow(&MODULE_P20, 6);

    /* 2. CAN module initialization. */
    IfxCan_Can_initModuleConfig(&canModuleConfig, &MODULE_CAN0);
    IfxCan_Can_initModule(&Can_Runtime.CanModule, &canModuleConfig);

    /* 3. CAN node configuration. */
    IfxCan_Can_initNodeConfig(&canNodeConfig, &Can_Runtime.CanModule);
    
    /* 4. Bit timing. */
    canNodeConfig.baudRate.baudrate                  = 500000U;
    canNodeConfig.fastBaudRate.baudrate              = 2000000U;
    canNodeConfig.busLoopbackEnabled = FALSE;

    /* 5. Node & Pins configuration. */
    canNodeConfig.nodeId = IfxCan_NodeId_0;
    canNodeConfig.pins   = &Can_Pins;

    /* 6. Frame configuration. */
    canNodeConfig.frame.type = IfxCan_FrameType_transmitAndReceive;
    canNodeConfig.frame.mode = IfxCan_FrameMode_fdLongAndFast;

    /* 7. Message RAM layout. */
    canNodeConfig.messageRAM.baseAddress                    = (uint32)CAN0_RAM;
    canNodeConfig.messageRAM.standardFilterListStartAddress = 0x100U;
    canNodeConfig.messageRAM.rxFifo0StartAddress            = 0x200U;
    canNodeConfig.messageRAM.txBuffersStartAddress          = 0x600U;

    /*
     * 8. Rx/Tx object allocation.
     *
     * Current project policy:
     *   - Use only Rx FIFO0.
     *   - Multiple RX HOHs may store frames into FIFO0.
     *   - After reading one frame from FIFO0, CanId is used to find the matching HRH.
     *
     * Extension point:
     *   - FIFO1 can be added later by assigning some RX HOHs to CAN_RX_DEST_FIFO1 and adding Can_ReadRxFifo1().
     *   - Dedicated Rx Buffer can be added later by assigning CAN_RX_DEST_BUFFER and adding buffer-index config.
     */
    canNodeConfig.filterConfig.standardListSize = CAN_STANDARD_FILTER_COUNT;
    canNodeConfig.filterConfig.standardFilterForNonMatchingFrames = (IfxCan_NonMatchingFrame)0;
    
    canNodeConfig.rxConfig.rxMode               = IfxCan_RxMode_fifo0;
    canNodeConfig.rxConfig.rxFifo0Size          = 14U;
    canNodeConfig.rxConfig.rxFifo0OperatingMode = IfxCan_RxFifoMode_blocking;
    canNodeConfig.rxConfig.rxFifo0DataFieldSize = IfxCan_DataFieldSize_64;

    canNodeConfig.txConfig.txFifoQueueSize          = 0U;
    canNodeConfig.txConfig.dedicatedTxBuffersNumber = 1U;
    canNodeConfig.txConfig.txBufferDataFieldSize    = IfxCan_DataFieldSize_64;
    
    /* 10. Interrupt disable. */
    canNodeConfig.interruptConfig.transmissionCompletedEnabled = FALSE;
    canNodeConfig.interruptConfig.rxFifo0NewMessageEnabled     = FALSE;

    /* 9. Node initialization. */
    IfxCan_Can_initNode(&Can_Runtime.Node[CAN_CONTROLLER_0], &canNodeConfig);

    /* 10. Standard filter initialization. */
    for (i = 0U; i < CAN_HOH_COUNT; i++)
    {
        if (Can_HardwareObjectConfig[i].CanObjectType == CAN_OBJECT_TYPE_RECEIVE)
        {
            Can_InitRxFilter(&Can_HardwareObjectConfig[i]);
        }
    }
}

Std_ReturnType Can_SetControllerMode(
    uint8 Controller,
    Can_ControllerStateType Transition
)
{
    if (Controller >= CAN_CONTROLLER_COUNT)
    {
        return E_NOT_OK;
    }

    switch (Transition)
    {
        case CAN_CS_STARTED:
        {
            if (Can_ControllerState[Controller] != CAN_CS_STOPPED)
            {
                return E_NOT_OK;
            }

            Can_ControllerState[Controller] = CAN_CS_STARTED;
            return E_OK;
        }

        case CAN_CS_STOPPED:
        {
            Can_ControllerState[Controller] = CAN_CS_STOPPED;
            return E_OK;
        }

        case CAN_CS_SLEEP:
        {
            if ((Can_ControllerState[Controller] != CAN_CS_STOPPED) &&
                (Can_ControllerState[Controller] != CAN_CS_SLEEP))
            {
                return E_NOT_OK;
            }

            Can_ControllerState[Controller] = CAN_CS_SLEEP;
            return E_OK;
        }

        default:
        {
            return E_NOT_OK;
        }
    }
}

Std_ReturnType Can_GetControllerMode(
    uint8 Controller,
    Can_ControllerStateType* ControllerModePtr
)
{
    if ((Controller >= CAN_CONTROLLER_COUNT) || (ControllerModePtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    *ControllerModePtr = Can_ControllerState[Controller];
    return E_OK;
}

Std_ReturnType Can_Write(
    Can_HwHandleType Hth,
    const Can_PduType* PduInfoPtr
)
{
    IfxCan_Message txMsg;
    uint32 txData[CAN_MAX_DATA_PAYLOAD_WORDS];
    IfxCan_Status status;
    const Can_HardwareObjectConfigType* HohConfig;

    if (Hth >= CAN_HOH_COUNT)
    {
        return E_NOT_OK;
    }

    if (PduInfoPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (PduInfoPtr->sdu == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (PduInfoPtr->length > CAN_MAX_DATA_PAYLOAD)
    {
        return E_NOT_OK;
    }

    CAN_DEBUG_PRINTF(
        "[Can][TX] Hth=%u CanId=0x%03X Len=%u Data=",
        (unsigned int)Hth,
        (unsigned int)PduInfoPtr->id,
        (unsigned int)PduInfoPtr->length
    );

    CAN_DEBUG_PRINT_DATA(
        PduInfoPtr->sdu,
        PduInfoPtr->length
    );

    HohConfig = &Can_HardwareObjectConfig[Hth];

    if (HohConfig->CanObjectType != CAN_OBJECT_TYPE_TRANSMIT)
    {
        return E_NOT_OK;
    }

    if (Can_ControllerState[HohConfig->CanControllerId] != CAN_CS_STARTED)
    {
        return E_NOT_OK;
    }

    if (Can_TxPending[Hth].IsPending == TRUE)
    {
        return E_NOT_OK;
    }

    IfxCan_Can_initMessage(&txMsg);

    memset((void*)txData, CAN_UNUSED_DATA_VALUE, sizeof(txData));
    memcpy((uint8*)txData, PduInfoPtr->sdu, PduInfoPtr->length);

    txMsg.bufferNumber    = HohConfig->ObjectConfig.Tx.CanTxBufferIndex;
    txMsg.messageId       = PduInfoPtr->id;
    txMsg.messageIdLength = IfxCan_MessageIdLength_standard;
    txMsg.frameMode       = IfxCan_FrameMode_fdLongAndFast;
    txMsg.dataLengthCode  = Can_ConvertLengthToDlc(PduInfoPtr->length);

    status = IfxCan_Can_sendMessage(
        &Can_Runtime.Node[HohConfig->CanControllerId],
        &txMsg,
        txData
    );

    if (status == IfxCan_Status_ok)
    {
        Can_TxPending[Hth].IsPending        = TRUE;
        Can_TxPending[Hth].SwPduHandle      = PduInfoPtr->swPduHandle;
        Can_TxPending[Hth].CanControllerId  = HohConfig->CanControllerId;
        Can_TxPending[Hth].CanTxBufferIndex = HohConfig->ObjectConfig.Tx.CanTxBufferIndex;

        return E_OK;
    }

    return E_NOT_OK;
}

void Can_MainFunction_Read(
    void
)
{
    Can_HwType  mailbox;
    PduInfoType pduInfo;
    uint8 sduData[CAN_MAX_DATA_PAYLOAD];

    if (Can_ControllerState[CAN_CONTROLLER_0] != CAN_CS_STARTED)
    {
        return;
    }

    pduInfo.SduDataPtr = sduData;
    pduInfo.SduLength  = 0U;

    /*
     * Current implementation reads only FIFO0.
     * Extension point: add a controller/Rx-destination loop when FIFO1 or dedicated Rx buffers are used.
     */
    while (Can_ReadRxFifo0(&mailbox, &pduInfo) == E_OK)
    {
        CanIf_RxIndication(&mailbox, &pduInfo);
        pduInfo.SduLength = 0U;
    }
}

void Can_MainFunction_Write(
    void
)
{
    uint8 hohIndex;

    if (Can_ControllerState[CAN_CONTROLLER_0] != CAN_CS_STARTED)
    {
        return;
    }

    for (hohIndex = 0U; hohIndex < CAN_HOH_COUNT; hohIndex++)
    {
        if (Can_TxPending[hohIndex].IsPending == TRUE)
        {
            if (IfxCan_Can_isTxBufferRequestPending(
                    &Can_Runtime.Node[Can_TxPending[hohIndex].CanControllerId],
                    Can_TxPending[hohIndex].CanTxBufferIndex) == FALSE)
            {
                Can_TxPending[hohIndex].IsPending = FALSE;

                CAN_DEBUG_PRINTF(
                    "[Can][TX-CNF] Hoh=%u SwPduHandle=%u Result=%u\r\n",
                    (unsigned int)hohIndex,
                    (unsigned int)Can_TxPending[hohIndex].SwPduHandle,
                    (unsigned int)E_OK
                );

                CanIf_TxConfirmation(
                    Can_TxPending[hohIndex].SwPduHandle,
                    E_OK
                );
            }
        }
    }
}

/*********************************************************************************************************************/
/*----------------------------------------------Private Implementations----------------------------------------------*/
/*********************************************************************************************************************/

/*
 * TXBRP (Tx Buffer Request Pending):
 *    The bits are set via register TXBAR. 
 *    The bits are reset after a requested transmission has completed 
 *    or has been cancelled via register TXBCR.
 * 
 * TXBAR (Tx Buffer Add Request):
 *   This enables the Host to set transmission requests 
 *   for multiple Tx Buffers with one write to TXBAR.
 *  
 * TXBTO (Tx Buffer Transmission Occured):
 *    The bits are set when the corresponding TXBRP bit is
 *    cleared after a successful transmission.
 *    The bits are reset when a new transmission is requested by writing a ‘1’
 *    to the corresponding bit of register TXBAR.
 */

static IfxCan_DataLengthCode Can_ConvertLengthToDlc(
    uint8 Length
)
{
    if (Length <= 8U)
    {
        return (IfxCan_DataLengthCode)Length;
    }
    else if (Length <= 12U)
    {
        return IfxCan_DataLengthCode_12;
    }
    else if (Length <= 16U)
    {
        return IfxCan_DataLengthCode_16;
    }
    else if (Length <= 20U)
    {
        return IfxCan_DataLengthCode_20;
    }
    else if (Length <= 24U)
    {
        return IfxCan_DataLengthCode_24;
    }
    else if (Length <= 32U)
    {
        return IfxCan_DataLengthCode_32;
    }
    else if (Length <= 48U)
    {
        return IfxCan_DataLengthCode_48;
    }
    else
    {
        return IfxCan_DataLengthCode_64;
    }
}

static uint8 Can_ConvertDlcToLength(
    IfxCan_DataLengthCode DataLengthCode
)
{
    static const uint8 Can_DlcToLengthMap[] =
    {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
        8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U
    };

    uint8 dlc = (uint8)DataLengthCode;

    if (dlc < (uint8)(sizeof(Can_DlcToLengthMap) / sizeof(Can_DlcToLengthMap[0])))
    {
        return Can_DlcToLengthMap[dlc];
    }

    return 0U;
}

static void Can_InitRxFilter(
    const Can_HardwareObjectConfigType* HohConfig
)
{
    IfxCan_Filter filter;

    if (HohConfig == NULL_PTR)
    {
        return;
    }

    if (HohConfig->CanObjectType != CAN_OBJECT_TYPE_RECEIVE)
    {
        return;
    }

    memset((void*)&filter, 0, sizeof(filter));

    filter.number = HohConfig->ObjectConfig.Rx.CanFilterIndex;
    filter.type   = IfxCan_FilterType_range;
    filter.id1    = HohConfig->ObjectConfig.Rx.CanFilterId1;
    filter.id2    = HohConfig->ObjectConfig.Rx.CanFilterId2;

    switch (HohConfig->ObjectConfig.Rx.CanRxDestination)
    {
        case CAN_RX_DEST_FIFO0:
        {
            filter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxFifo0;
            break;
        }

        case CAN_RX_DEST_FIFO1:
        {
            /*
             * Extension point:
             * FIFO1 is not used in the current project configuration.
             * Enable rxFifo1 Message RAM and add Can_ReadRxFifo1() before using this path.
             */
            filter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxFifo1;
            break;
        }

        case CAN_RX_DEST_BUFFER:
        {
            /*
             * Extension point:
             * Dedicated Rx Buffer is not used in the current project configuration.
             * Add Rx buffer index config before using this path.
             */
            filter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
            filter.rxBufferOffset = 0U;
            break;
        }

        default:
        {
            return;
        }
    }

    IfxCan_Can_setStandardFilter(
        &Can_Runtime.Node[HohConfig->CanControllerId],
        &filter
    );
}

static Std_ReturnType Can_ReadRxFifo0(
     Can_HwType* Mailbox,
    PduInfoType* PduInfoPtr
)
{
    IfxCan_Message rxMsg;
    uint32 rxData[CAN_MAX_DATA_PAYLOAD_WORDS];
    uint8 rxLength;
    Can_HwHandleType hoh;
    const Can_HardwareObjectConfigType* hohConfig;

    if ((Mailbox == NULL_PTR) || (PduInfoPtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (PduInfoPtr->SduDataPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (Can_Runtime.Node[CAN_CONTROLLER_0].node->RX.F0S.B.F0FL == 0U)
    {
        return E_NOT_OK;
    }

    IfxCan_Can_initMessage(&rxMsg);
    memset((void*)rxData, CAN_UNUSED_DATA_VALUE, sizeof(rxData));

    rxMsg.readFromRxFifo0 = TRUE;

    IfxCan_Can_readMessage(
        &Can_Runtime.Node[CAN_CONTROLLER_0],
        &rxMsg,
        rxData
    );

    rxLength = Can_ConvertDlcToLength(rxMsg.dataLengthCode);

    if ((rxLength == 0U) || (rxLength > CAN_MAX_DATA_PAYLOAD))
    {
        return E_NOT_OK;
    }

    if (Can_FindRxHohByCanId(rxMsg.messageId, &hoh) != E_OK)
    {
        return E_NOT_OK;
    }

    hohConfig = &Can_HardwareObjectConfig[hoh];

    Mailbox->CanId        = rxMsg.messageId;
    Mailbox->Hoh          = hoh;
    Mailbox->ControllerId = hohConfig->CanControllerId;

    PduInfoPtr->SduLength = rxLength;

    memset(
        (void*)PduInfoPtr->SduDataPtr,
        CAN_UNUSED_DATA_VALUE,
        CAN_MAX_DATA_PAYLOAD
    );

    memcpy(
        (void*)PduInfoPtr->SduDataPtr,
        (const void*)rxData,
        rxLength
    );

    CAN_DEBUG_PRINTF(
        "[Can][RX] Hoh=%u CanId=0x%03X Len=%u Data=",
        (unsigned int)Mailbox->Hoh,
        (unsigned int)Mailbox->CanId,
        (unsigned int)PduInfoPtr->SduLength
    );

    CAN_DEBUG_PRINT_DATA(
        PduInfoPtr->SduDataPtr,
        PduInfoPtr->SduLength
    );

    return E_OK;
}

static Std_ReturnType Can_FindRxHohByCanId(
    Can_IdType CanId,
    Can_HwHandleType* HrhPtr
)
{
    uint8 hohIndex;

    if (HrhPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    for (hohIndex = 0U; hohIndex < CAN_HOH_COUNT; hohIndex++)
    {
        const Can_HardwareObjectConfigType* HohConfig = &Can_HardwareObjectConfig[hohIndex];

        if (HohConfig->CanObjectType != CAN_OBJECT_TYPE_RECEIVE)
        {
            continue;
        }

        /*
         * Current filter type is range filter.
         * Extension point: if mask/dual filters are added later, add filter-type config and branch here.
         */
        if ((CanId >= HohConfig->ObjectConfig.Rx.CanFilterId1) &&
            (CanId <= HohConfig->ObjectConfig.Rx.CanFilterId2))
        {
            *HrhPtr = HohConfig->CanObjectId;
            return E_OK;
        }
    }

    return E_NOT_OK;
}
