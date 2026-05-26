/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "Can_Cfg.h"

/*********************************************************************************************************************/
/*------------------------------------------------Controller Configs------------------------------------------------*/
/*********************************************************************************************************************/

const Can_ControllerConfigType Can_ControllerConfig[CAN_CONTROLLER_COUNT] =
{
    {
        .CanControllerId = CAN_CONTROLLER_0,
        .CanNodeId       = 0U,
        .CanFdEnabled    = TRUE
    }
};

/*********************************************************************************************************************/
/*---------------------------------------------Hardware Object Configs----------------------------------------------*/
/*********************************************************************************************************************/

const Can_HardwareObjectConfigType Can_HardwareObjectConfig[CAN_HOH_COUNT] =
{
    /*
     * Gateway ECU -> Target ECU 방향 송신 object.
     *
     * 이 object는 ISO-TP의 SF/FF/CF 송신뿐 아니라,
     * Target ECU가 보낸 FF에 대한 FC 송신에도 사용될 수 있다.
     */
    {
        .CanObjectId            = CAN_HOH_LOCAL_TO_REMOTE_TX,
        .CanObjectType          = CAN_OBJECT_TYPE_TRANSMIT,
        .CanControllerId        = CAN_CONTROLLER_0,
        .CanObjectPayloadLength = CAN_MAX_DATA_PAYLOAD,

        .ObjectConfig.Tx =
        {
            .CanTxBufferIndex = 0U
        }
    },

    /*
     * Target ECU -> Gateway ECU 방향 수신 object.
     *
     * 수신된 frame이 SF/FF/CF/FC 중 무엇인지는
     * CAN Driver가 아니라 CanTp가 ISO-TP PCI 값을 보고 판단한다.
     */
    {
        .CanObjectId            = CAN_HOH_REMOTE_TO_LOCAL_RX,
        .CanObjectType          = CAN_OBJECT_TYPE_RECEIVE,
        .CanControllerId        = CAN_CONTROLLER_0,
        .CanObjectPayloadLength = CAN_MAX_DATA_PAYLOAD,

        .ObjectConfig.Rx =
        {
            .CanRxDestination = CAN_RX_DEST_FIFO0,
            .CanFilterIndex   = 0U,
            .CanFilterId1     = 0x7E8U,
            .CanFilterId2     = 0x7E8U
        }
    }
};