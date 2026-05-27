#ifndef SOTA_UPDATEMANAGER_H_
#define SOTA_UPDATEMANAGER_H_

#include "Ifx_Types.h"

#include "../Metadata/Sota_Metadata.h"
#include "../Transport/Sota_CanFdTransport.h"

#define SOTA_UPDATE_ERROR_VERIFY_FAILED          (0x53560100U)
#define SOTA_UPDATE_ERROR_SWAP_ARM_FAILED        (0x53570100U)
#define SOTA_UPDATE_ERROR_SWAP_LOG_FULL          (0x53570101U)
#define SOTA_UPDATE_ERROR_BOOT_BANK_MISMATCH     (0x53420100U)
#define SOTA_UPDATE_ERROR_BOOT_ATTEMPT_EXCEEDED  (0x53420101U)
#define SOTA_UPDATE_ERROR_COMMIT_INVALID_STATE   (0x53430100U)

typedef enum
{
    SOTA_UPDATE_MANAGER_STATE_IDLE = 0,
    SOTA_UPDATE_MANAGER_STATE_SESSION_OPEN,
    SOTA_UPDATE_MANAGER_STATE_DOWNLOAD_REQUESTED,
    SOTA_UPDATE_MANAGER_STATE_ERASE_PENDING,
    SOTA_UPDATE_MANAGER_STATE_ERASING,
    SOTA_UPDATE_MANAGER_STATE_RECEIVING,
    SOTA_UPDATE_MANAGER_STATE_WRITE_PENDING,
    SOTA_UPDATE_MANAGER_STATE_PROGRAMMING,
    SOTA_UPDATE_MANAGER_STATE_TRANSFER_EXIT_RECEIVED,
    SOTA_UPDATE_MANAGER_STATE_VERIFY_PENDING,
    SOTA_UPDATE_MANAGER_STATE_VERIFYING,
    SOTA_UPDATE_MANAGER_STATE_VERIFIED,
    SOTA_UPDATE_MANAGER_STATE_SWAP_ARM_REQUESTED,
    SOTA_UPDATE_MANAGER_STATE_SWAP_ARMED,
    SOTA_UPDATE_MANAGER_STATE_TESTING_AFTER_BOOT,
    SOTA_UPDATE_MANAGER_STATE_COMMITTED,
    SOTA_UPDATE_MANAGER_STATE_ROLLBACK_REQUESTED,
    SOTA_UPDATE_MANAGER_STATE_ROLLBACK_ARMED,
    SOTA_UPDATE_MANAGER_STATE_ROLLED_BACK,
    SOTA_UPDATE_MANAGER_STATE_ABORTED,
    SOTA_UPDATE_MANAGER_STATE_ERROR
} SotaUpdateManager_State_t;

void SotaUpdateManager_Init(void);
SotaCanFd_ResponseCode_t SotaUpdateManager_HandleBegin(const SotaCanFd_BeginRequest_t *request);
SotaCanFd_ResponseCode_t SotaUpdateManager_HandleData(const SotaCanFd_DataRequest_t *request);
SotaCanFd_ResponseCode_t SotaUpdateManager_HandleEnd(const SotaCanFd_ControlRequest_t *request);
SotaCanFd_ResponseCode_t SotaUpdateManager_HandleStatus(const SotaCanFd_ControlRequest_t *request,
                                                        SotaCanFd_StatusResponse_t *outStatus);
SotaCanFd_ResponseCode_t SotaUpdateManager_HandleVerify(const SotaCanFd_ControlRequest_t *request);
SotaCanFd_ResponseCode_t SotaUpdateManager_HandleActivate(const SotaCanFd_ControlRequest_t *request);
SotaCanFd_ResponseCode_t SotaUpdateManager_HandleCommit(const SotaCanFd_ControlRequest_t *request);
SotaCanFd_ResponseCode_t SotaUpdateManager_HandleRollback(const SotaCanFd_ControlRequest_t *request);
SotaCanFd_ResponseCode_t SotaUpdateManager_HandleAbort(const SotaCanFd_ControlRequest_t *request);
void SotaUpdateManager_RunStep(void);
SotaCanFd_ResponseCode_t SotaUpdateManager_GetStatus(SotaCanFd_StatusResponse_t *outStatus);
void SotaUpdateManager_RestoreFromMetadata(const SotaMetadata_Record_t *record);

#endif /* SOTA_UPDATEMANAGER_H_ */
