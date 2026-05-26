#ifndef CANTP_CFG_H_
#define CANTP_CFG_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "CanTp.h"
#include "CanIf_Cfg.h"
#include "PduR_Cfg.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Counts-------------------------------------------------------*/
/*********************************************************************************************************************/

#define CANTP_TXNSDU_COUNT                  (1U)
#define CANTP_RXNSDU_COUNT                  (1U)

/*********************************************************************************************************************/
/*----------------------------------------------------N-SDU IDs------------------------------------------------------*/
/*********************************************************************************************************************/

/*
 * CanTp N-SDU IDs
 *
 * N-SDU는 CanTp가 segmentation/reassembly 대상으로 다루는
 * 전체 transport payload 단위이다.
 *
 * CanTp는 이 payload가 UDS 요청인지 응답인지 해석하지 않는다.
 * 여기서는 ISO-TP connection의 방향만 표현한다.
 */
#define CANTP_TXNSDU_LOCAL_TO_REMOTE        (0U)
#define CANTP_RXNSDU_REMOTE_TO_LOCAL        (0U)

/*********************************************************************************************************************/
/*----------------------------------------------------N-PDU IDs------------------------------------------------------*/
/*********************************************************************************************************************/

/*
 * CanTp N-PDU IDs
 *
 * N-PDU는 CanIf와 CanTp 사이에서 오가는 CAN frame 단위의 handle이다.
 * SF/FF/CF/FC 구분은 CAN ID나 PDU ID가 아니라 ISO-TP PCI 값으로 판단한다.
 */
#define CANTP_TXNPDU_LOCAL_TO_REMOTE        (0U)
#define CANTP_RXNPDU_REMOTE_TO_LOCAL        (0U)

/*********************************************************************************************************************/
/*--------------------------------------------------Frame Constants--------------------------------------------------*/
/*********************************************************************************************************************/

#define CANTP_CAN_FRAME_LENGTH              (8U)

#define CANTP_SF_MAX_PAYLOAD_LENGTH         (7U)
#define CANTP_FF_DATA_LENGTH                (6U)
#define CANTP_CF_DATA_LENGTH                (7U)

#define CANTP_RX_BUFFER_SIZE                (1024U)
#define CANTP_TX_BUFFER_SIZE                (1024U)

/*********************************************************************************************************************/
/*--------------------------------------------------PCI Constants----------------------------------------------------*/
/*********************************************************************************************************************/

#define CANTP_PCI_TYPE_SF                   (0x00U)
#define CANTP_PCI_TYPE_FF                   (0x10U)
#define CANTP_PCI_TYPE_CF                   (0x20U)
#define CANTP_PCI_TYPE_FC                   (0x30U)

#define CANTP_PCI_TYPE_MASK                 (0xF0U)
#define CANTP_PCI_LENGTH_MASK               (0x0FU)
#define CANTP_PCI_SN_MASK                   (0x0FU)

/*********************************************************************************************************************/
/*------------------------------------------------Flow Control Constants---------------------------------------------*/
/*********************************************************************************************************************/

#define CANTP_FC_STATUS_CTS                 (0x00U)
#define CANTP_FC_STATUS_WAIT                (0x01U)
#define CANTP_FC_STATUS_OVERFLOW            (0x02U)

#define CANTP_DEFAULT_BLOCK_SIZE            (0U)
#define CANTP_DEFAULT_STMIN                 (0U)
#define CANTP_MAX_WAIT_FRAME_COUNT          (3U)

/*********************************************************************************************************************/
/*------------------------------------------------------Types--------------------------------------------------------*/
/*********************************************************************************************************************/

/*
 * CanTp_TxNsduConfigType
 *
 * CanTp 기준 Tx N-SDU 설정이다.
 *
 * CanTpTxNsduId:
 *   상위 계층(PduR)이 CanTp_Transmit()을 호출할 때 사용하는
 *   CanTp 기준 송신 N-SDU handle.
 *
 * CanIfTxNpduId:
 *   CanTp가 SF/FF/CF를 실제 CAN frame으로 송신할 때 사용할
 *   CanIf 기준 Tx L-PDU handle.
 *
 * PduRTxPduId:
 *   전체 TP N-SDU 송신 완료/실패를 PduR로 알릴 때 사용하는
 *   PduR-facing Tx confirmation PDU handle.
 */
typedef struct
{
    PduIdType CanTpTxNsduId;
    PduIdType CanIfTxNpduId;
    PduIdType PduRTxPduId;
} CanTp_TxNsduConfigType;

/*
 * CanTp_RxNsduConfigType
 *
 * CanTp 기준 Rx N-SDU 설정이다.
 *
 * CanTpRxNsduId:
 *   CanIf가 CanTp_RxIndication()을 호출할 때 전달하는
 *   CanTp 기준 수신 N-SDU handle.
 *
 * CanIfRxNpduId:
 *   CanIf에서 올라오는 CAN frame 단위 Rx L-PDU handle.
 *
 * CanIfTxFcPduId:
 *   FF 수신 후 CanTp가 Flow Control frame을 송신할 때 사용할
 *   CanIf 기준 Tx L-PDU handle.
 *
 * PduRRxPduId:
 *   전체 TP N-SDU 수신 완료/실패를 PduR로 알릴 때 사용하는
 *   PduR-facing Rx PDU handle.
 *
 * RxBufferSize:
 *   재조립에 사용할 수신 buffer 크기.
 *
 * BlockSize:
 *   Flow Control CTS에서 송신자에게 허용할 Consecutive Frame 개수.
 *   0이면 block 제한 없이 남은 CF를 허용한다.
 *
 * STmin:
 *   Flow Control CTS에서 송신자에게 요구할 Consecutive Frame 간 최소 간격.
 */
typedef struct
{
    PduIdType CanTpRxNsduId;
    PduIdType CanIfRxNpduId;
    PduIdType CanIfTxFcPduId;
    PduIdType PduRRxPduId;
    uint16    RxBufferSize;
    uint8     BlockSize;
    uint8     STmin;
} CanTp_RxNsduConfigType;

/*********************************************************************************************************************/
/*---------------------------------------------------Extern Configs--------------------------------------------------*/
/*********************************************************************************************************************/

extern const CanTp_TxNsduConfigType CanTp_TxNsduConfig[CANTP_TXNSDU_COUNT];
extern const CanTp_RxNsduConfigType CanTp_RxNsduConfig[CANTP_RXNSDU_COUNT];

#endif /* CANTP_CFG_H_ */
