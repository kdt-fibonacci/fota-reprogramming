#ifndef DOIP_CFG_H_
#define DOIP_CFG_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

#define DOIP_PROTOCOL_VERSION                          (0x02U)
#define DOIP_INVERSE_PROTOCOL_VERSION                  (0xFDU)

#define DOIP_HEADER_LENGTH                             (8U)

#define DOIP_RX_BUFFER_SIZE                            (2048U)
#define DOIP_TX_BUFFER_SIZE                            (2048U)

/*********************************************************************************************************************/
/*------------------------------------------------Logical Addresses--------------------------------------------------*/
/*********************************************************************************************************************/

#define DOIP_LOGICAL_ADDRESS_ECU                       (0x0F00U)

/*
 * Gateway 뒤쪽 CAN Target ECU Logical Address
 *
 * DoIP Diagnostic Message의 TargetAddress가 이 값이면
 * DoIP는 해당 메시지를 CanTp 방향으로 라우팅하기 위한 DoIPRxPduId로 변환한다.
 */
#define DOIP_LOGICAL_ADDRESS_TARGET_ECU_0              (0x1234U)
#define DOIP_LOGICAL_ADDRESS_TARGET_ECU_1              (0x5678U)

/*
 * Tester Logical Address 기본값
 */
#define DOIP_LOGICAL_ADDRESS_TESTER                    (0x0E00U)

/*********************************************************************************************************************/
/*--------------------------------------------------Payload Types----------------------------------------------------*/
/*********************************************************************************************************************/

#define DOIP_PAYLOAD_TYPE_GENERIC_NACK                 (0x0000U)
#define DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQ       (0x0005U)
#define DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RES       (0x0006U)
#define DOIP_PAYLOAD_TYPE_DIAG_MESSAGE                 (0x8001U)
#define DOIP_PAYLOAD_TYPE_DIAG_POS_ACK                 (0x8002U)
#define DOIP_PAYLOAD_TYPE_DIAG_NEG_ACK                 (0x8003U)

/*********************************************************************************************************************/
/*--------------------------------------------------NACK Codes-------------------------------------------------------*/
/*********************************************************************************************************************/

#define DOIP_GENERIC_NACK_INVALID_HEADER               (0x00U)
#define DOIP_GENERIC_NACK_UNKNOWN_PAYLOAD_TYPE         (0x01U)
#define DOIP_GENERIC_NACK_MESSAGE_TOO_LARGE            (0x02U)

#define DOIP_DIAG_ACK_CODE_OK                          (0x00U)
#define DOIP_DIAG_NACK_CODE_UNKNOWN_TARGET             (0x02U)
#define DOIP_DIAG_NACK_CODE_MESSAGE_TOO_LARGE          (0x04U)

/*********************************************************************************************************************/
/*---------------------------------------------Routing Activation Codes----------------------------------------------*/
/*********************************************************************************************************************/

#define DOIP_ROUTING_ACTIVATION_RES_SUCCESS            (0x10U)
#define DOIP_ROUTING_ACTIVATION_RES_DENIED             (0x11U)

/*********************************************************************************************************************/
/*---------------------------------------------------PDU IDs---------------------------------------------------------*/
/*********************************************************************************************************************/

/*
 * DoIP Rx PDU ID
 *
 * 현재 구조에서는 Gateway 자체 진단을 하지 않으므로
 * DoIP Rx 경로는 ECU0/ECU1로 향하는 CanTp 방향만 존재한다.
 */
#define DOIP_RXPDU_DIAG_REQ_TO_CANTP_ECU0              (0U)
#define DOIP_RXPDU_DIAG_REQ_TO_CANTP_ECU1              (1U)
#define DOIP_RXPDU_COUNT                               (2U)

/*
 * DoIP Tx PDU ID
 *
 * 현재 구조에서는 ECU0/ECU1에서 올라온 UDS Response를
 * DoIP Diagnostic Message로 감싸 Tester에게 돌려준다.
 */
#define DOIP_TXPDU_DIAG_RES_FROM_CANTP_ECU0            (0U)
#define DOIP_TXPDU_DIAG_RES_FROM_CANTP_ECU1            (1U)
#define DOIP_TXPDU_COUNT                               (2U)

/*********************************************************************************************************************/
/*------------------------------------------------------Types--------------------------------------------------------*/
/*********************************************************************************************************************/

typedef struct
{
    uint16 EntityLogicalAddress;
    uint16 TesterLogicalAddress;

    PduIdType SoAdRxPduId;
    PduIdType SoAdTxPduId;
} DoIP_ConfigType;

typedef struct
{
    PduIdType DoIPRxPduId;
    uint16 TargetAddress;
} DoIP_RxPduConfigType;

typedef struct
{
    PduIdType DoIPTxPduId;
    uint16 SourceAddress;
} DoIP_TxPduConfigType;

/*********************************************************************************************************************/
/*------------------------------------------------Global Constants---------------------------------------------------*/
/*********************************************************************************************************************/

extern const DoIP_ConfigType DoIP_Config;

extern const DoIP_RxPduConfigType DoIP_RxPduConfig[DOIP_RXPDU_COUNT];

extern const DoIP_TxPduConfigType DoIP_TxPduConfig[DOIP_TXPDU_COUNT];

#endif /* DOIP_CFG_H_ */
