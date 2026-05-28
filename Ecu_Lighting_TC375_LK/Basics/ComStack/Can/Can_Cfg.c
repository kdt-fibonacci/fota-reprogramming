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
    {
        .CanObjectId            = CAN_HOH_LOCAL_TO_REMOTE_TX,
        .CanObjectType          = CAN_OBJECT_TYPE_TRANSMIT,
        .CanControllerId        = CAN_CONTROLLER_0,
        .CanObjectPayloadLength = CAN_MAX_DATA_PAYLOAD,

        .ObjectConfig =
        {
            .Tx =
            {
                .CanTxBufferIndex = 0U
            }
        }
    },

    {
        .CanObjectId            = CAN_HOH_REMOTE_TO_LOCAL_RX,
        .CanObjectType          = CAN_OBJECT_TYPE_RECEIVE,
        .CanControllerId        = CAN_CONTROLLER_0,
        .CanObjectPayloadLength = CAN_MAX_DATA_PAYLOAD,

        .ObjectConfig =
        {
            .Rx =
            {
                .CanRxDestination = CAN_RX_DEST_FIFO0,
                .CanFilterIndex   = 0U,
                .CanFilterId1     = CAN_ID_REMOTE_TO_LOCAL,
                .CanFilterId2     = CAN_ID_REMOTE_TO_LOCAL
            }
        }
    }
};
