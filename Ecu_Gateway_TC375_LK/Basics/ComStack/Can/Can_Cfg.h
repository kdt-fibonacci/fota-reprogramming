#ifndef CAN_CFG_H_
#define CAN_CFG_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "Can.h"
#include "Platform_Types.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Counts-------------------------------------------------------*/
/*********************************************************************************************************************/

#define CAN_CONTROLLER_COUNT              (1U)
#define CAN_HOH_COUNT                     (3U)

#define CAN_STANDARD_FILTER_COUNT         (2U)
#define CAN_EXTENDED_FILTER_COUNT         (0U)

/*********************************************************************************************************************/
/*---------------------------------------------------Controller IDs--------------------------------------------------*/
/*********************************************************************************************************************/

#define CAN_CONTROLLER_0                  (0U)

/*********************************************************************************************************************/
/*-----------------------------------------------------CAN IDs-------------------------------------------------------*/
/*********************************************************************************************************************/

#define CAN_ID_GATEWAY_MOTION               (0x500U)
#define CAN_ID_MOTION                       (0x501U)
#define CAN_ID_GATEWAY_LIGHTING               (0x600U)
#define CAN_ID_LIGHTING                       (0x601U)

#define CAN_ID_GATEWAY                    (CAN_ID_GATEWAY_MOTION)

/*********************************************************************************************************************/
/*------------------------------------------------Hardware Object IDs-----------------------------------------------*/
/*********************************************************************************************************************/

/*
 * CanTp 아래 계층에서는 진단 요청/응답/FC 의미를 구분하지 않는다.
 * CAN 계층은 방향별 Hardware Object만 관리한다.
 */
#define CAN_HOH_LOCAL_TO_REMOTE_TX        (0U)
#define CAN_HOH_MOTION_TO_GATEWAY_RX        (1U)
#define CAN_HOH_LIGHTING_TO_GATEWAY_RX        (2U)

/*
 * Readability aliases.
 */
#define CAN_HTH_LOCAL_TO_REMOTE            (CAN_HOH_LOCAL_TO_REMOTE_TX)
#define CAN_HRH_MOTION_TO_GATEWAY            (CAN_HOH_MOTION_TO_GATEWAY_RX)
#define CAN_HRH_LIGHTING_TO_GATEWAY            (CAN_HOH_LIGHTING_TO_GATEWAY_RX)
#define CAN_HRH_REMOTE_TO_LOCAL            (CAN_HRH_MOTION_TO_GATEWAY)

/*********************************************************************************************************************/
/*------------------------------------------------------General------------------------------------------------------*/
/*********************************************************************************************************************/

#define CAN_MAX_DATA_PAYLOAD              (64U)
#define CAN_MAX_DATA_PAYLOAD_WORDS        (CAN_MAX_DATA_PAYLOAD / 4U)
#define CAN_UNUSED_DATA_VALUE             (0x11U)

#define CAN_TX_PIN                        (IfxCan_TXD00_P20_8_OUT)
#define CAN_RX_PIN                        (IfxCan_RXD00B_P20_7_IN)

/*********************************************************************************************************************/
/*------------------------------------------------------Types--------------------------------------------------------*/
/*********************************************************************************************************************/

typedef enum
{
    CAN_OBJECT_TYPE_TRANSMIT = 0U,
    CAN_OBJECT_TYPE_RECEIVE
} Can_ObjectType;

typedef enum
{
    CAN_RX_DEST_FIFO0 = 0U,

    /*
     * 현재 구현은 FIFO0 하나만 사용한다.
     * 추후 여러 수신 경로를 분리해야 하면 FIFO1 또는 Rx Buffer를 확장할 수 있다.
     */
    CAN_RX_DEST_FIFO1,
    CAN_RX_DEST_BUFFER
} Can_RxDestinationType;

typedef struct
{
    uint8   CanControllerId;
    uint8   CanNodeId;
    boolean CanFdEnabled;
} Can_ControllerConfigType;

typedef struct
{
    uint8 CanTxBufferIndex;
} Can_TxObjectConfigType;

typedef struct
{
    Can_RxDestinationType CanRxDestination;
    uint8                 CanFilterIndex;
    uint32                CanFilterId1;
    uint32                CanFilterId2;
} Can_RxObjectConfigType;

/*
 * Can_HardwareObjectConfigType
 *
 * CAN Driver 기준 Hardware Object 설정이다.
 *
 * CanObjectId:
 *   CAN Driver 내부에서 사용하는 Hardware Object Handle.
 *   송신 object이면 HTH, 수신 object이면 HRH 역할을 한다.
 *
 * CanObjectType:
 *   Hardware Object의 방향을 나타낸다.
 *   TRANSMIT이면 HTH, RECEIVE이면 HRH로 사용된다.
 *
 * CanControllerId:
 *   해당 Hardware Object가 연결된 CAN Controller ID.
 *
 * CanObjectPayloadLength:
 *   해당 Hardware Object에서 지원하는 최대 payload length.
 *
 * ObjectConfig.Tx.CanTxBufferIndex:
 *   송신 object에서 사용할 MCMCAN Tx Buffer index.
 *
 * ObjectConfig.Rx.CanRxDestination:
 *   수신 object에서 사용할 Rx destination.
 *   현재는 FIFO0만 사용한다.
 *
 * ObjectConfig.Rx.CanFilterIndex:
 *   MCMCAN Standard Filter table index.
 *
 * ObjectConfig.Rx.CanFilterId1 / CanFilterId2:
 *   수신 CAN ID filter 설정값.
 *   현재는 range filter 기준으로 사용한다.
 */
typedef struct
{
    Can_HwHandleType CanObjectId;
    Can_ObjectType   CanObjectType;
    uint8            CanControllerId;
    uint8            CanObjectPayloadLength;

    union
    {
        Can_TxObjectConfigType Tx;
        Can_RxObjectConfigType Rx;
    } ObjectConfig;
} Can_HardwareObjectConfigType;

/*********************************************************************************************************************/
/*---------------------------------------------------Extern Configs--------------------------------------------------*/
/*********************************************************************************************************************/

extern const Can_ControllerConfigType Can_ControllerConfig[CAN_CONTROLLER_COUNT];
extern const Can_HardwareObjectConfigType Can_HardwareObjectConfig[CAN_HOH_COUNT];

#endif /* CAN_CFG_H_ */
