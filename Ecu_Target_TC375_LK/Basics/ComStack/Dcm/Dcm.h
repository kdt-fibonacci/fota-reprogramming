#ifndef DCM_H_
#define DCM_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "Std_Types.h"
#include "ComStack_Types.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Types--------------------------------------------------------*/
/*********************************************************************************************************************/

/*
 * Dcm_OpStatusType
 *
 * 추후 Flash Write, Verify Routine 등 비동기 처리용 Callout을 붙일 때 사용할 수 있다.
 * 현재 기본 DCM 구현에서는 즉시 처리 중심이므로 직접적으로 많이 사용하지 않는다.
 */
typedef enum
{
    DCM_OP_INITIAL = 0U,
    DCM_OP_PENDING
} Dcm_OpStatusType;

/*
 * Dcm_ReturnWriteMemoryType
 *
 * 추후 TransferData 수신 데이터를 실제 Flash memory에 기록할 때
 * Memory Write Callout의 반환값으로 사용할 수 있다.
 */
typedef enum
{
    DCM_WRITE_OK = 0U,
    DCM_WRITE_PENDING,
    DCM_WRITE_FAILED
} Dcm_ReturnWriteMemoryType;

/*
 * Dcm_SessionType
 *
 * Target ECU의 현재 UDS diagnostic session 상태를 나타낸다.
 */
typedef enum
{
    DCM_SESSION_DEFAULT     = 0x01U,
    DCM_SESSION_PROGRAMMING = 0x02U,
    DCM_SESSION_EXTENDED    = 0x03U
} Dcm_SessionType;

/*
 * Dcm_FotaStateType
 *
 * Target ECU 내부의 FOTA 진행 상태를 나타낸다.
 *
 * 현재 구현 흐름:
 * IDLE
 * → EXTENDED_SESSION
 * → DOWNLOAD_ACCEPTED
 * → TRANSFER_IN_PROGRESS
 * → TRANSFER_COMPLETED
 * → VERIFIED
 * → PROGRAMMING_SESSION
 * → ACTIVATION_PENDING
 * → ACTIVATED
 *
 * ROLLBACK_DONE은 새 다운로드를 시작할 때 EXTENDED_SESSION으로 재진입할 수 있다.
 */
typedef enum
{
    DCM_FOTA_STATE_IDLE = 0U,

    DCM_FOTA_STATE_EXTENDED_SESSION,

    DCM_FOTA_STATE_DOWNLOAD_ACCEPTED,

    DCM_FOTA_STATE_TRANSFER_IN_PROGRESS,

    DCM_FOTA_STATE_TRANSFER_COMPLETED,

    DCM_FOTA_STATE_VERIFY_IN_PROGRESS,

    DCM_FOTA_STATE_VERIFIED,

    DCM_FOTA_STATE_PROGRAMMING_SESSION,

    DCM_FOTA_STATE_ACTIVATION_PENDING,

    DCM_FOTA_STATE_ACTIVATED,

    DCM_FOTA_STATE_ROLLBACK_PENDING,

    DCM_FOTA_STATE_ROLLBACK_DONE,

    DCM_FOTA_STATE_FAILED
} Dcm_FotaStateType;

/*
 * Dcm_FotaResultType
 *
 * 마지막 FOTA 처리 결과를 나타낸다.
 */
typedef enum
{
    DCM_FOTA_RESULT_NONE = 0U,

    DCM_FOTA_RESULT_SUCCESS,

    DCM_FOTA_RESULT_DOWNLOAD_FAILED,

    DCM_FOTA_RESULT_TRANSFER_FAILED,

    DCM_FOTA_RESULT_VERIFY_FAILED,

    DCM_FOTA_RESULT_ACTIVATION_FAILED,

    DCM_FOTA_RESULT_ROLLBACK_DONE,

    DCM_FOTA_RESULT_FAILED
} Dcm_FotaResultType;

/*********************************************************************************************************************/
/*------------------------------------------------------APIs---------------------------------------------------------*/
/*********************************************************************************************************************/

/*
 * DCM 모듈 초기화 함수.
 *
 * 초기 상태:
 * - Session: Default Session
 * - FOTA State: IDLE
 * - FOTA Result: NONE
 */
void Dcm_Init(void);

/*
 * PduR이 수신 완료된 UDS Request를 DCM에 전달할 때 호출한다.
 *
 * DcmRxPduId:
 *   DCM 기준 Rx I-PDU handle.
 *
 * PduInfoPtr:
 *   순수 UDS payload.
 *   DoIP address field 또는 CanTp PCI는 포함하지 않는다.
 *
 * 현재 구현에서는 이 함수에서 요청을 복사만 하고,
 * 실제 UDS service 처리는 Dcm_MainFunction()에서 수행한다.
 */
void Dcm_RxIndication(
    PduIdType DcmRxPduId,
    const PduInfoType* PduInfoPtr
);

/*
 * PduR이 DCM이 요청한 UDS Response 송신 완료/실패를 알릴 때 호출한다.
 *
 * DcmTxPduId:
 *   DCM 기준 Tx I-PDU handle.
 *
 * Result:
 *   E_OK 또는 E_NOT_OK.
 */
void Dcm_TxConfirmation(
    PduIdType DcmTxPduId,
    Std_ReturnType Result
);

/*
 * DCM 주기 함수.
 *
 * Dcm_RxIndication()에서 pending 처리된 UDS Request를 실제로 처리한다.
 * App main loop 또는 scheduler에서 주기적으로 호출하면 된다.
 */
void Dcm_MainFunction(void);

/*
 * 현재 Diagnostic Session을 반환한다.
 */
Dcm_SessionType Dcm_GetCurrentSession(void);

/*
 * 현재 FOTA 상태를 반환한다.
 */
Dcm_FotaStateType Dcm_GetFotaState(void);

/*
 * 마지막 FOTA 결과를 반환한다.
 */
Dcm_FotaResultType Dcm_GetLastFotaResult(void);

#endif /* DCM_H_ */
