#ifndef FOTA_HANDLER_H_
#define FOTA_HANDLER_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "Dcm.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

#ifndef FOTA_MAX_TRANSFER_DATA_LENGTH
#define FOTA_MAX_TRANSFER_DATA_LENGTH           (512U)
#endif

#ifndef FOTA_ALLOW_ACTIVATE_WITHOUT_VERIFY
#define FOTA_ALLOW_ACTIVATE_WITHOUT_VERIFY      (0U)
#endif

/*********************************************************************************************************************/
/*------------------------------------------------------Types--------------------------------------------------------*/
/*********************************************************************************************************************/

typedef enum
{
    FOTA_CHUNK_IDLE = 0U,
    FOTA_CHUNK_RECEIVED,
    FOTA_CHUNK_PROCESSING,
    FOTA_CHUNK_DONE,
    FOTA_CHUNK_ERROR
} FotaChunkStateType;

typedef enum
{
    FOTA_HANDLER_STATE_UNINIT = 0U,
    FOTA_HANDLER_STATE_IDLE,
    FOTA_HANDLER_STATE_DOWNLOAD_STARTED,
    FOTA_HANDLER_STATE_TRANSFERRING,
    FOTA_HANDLER_STATE_VERIFIED,
    FOTA_HANDLER_STATE_ACTIVATION_ARMED,
    FOTA_HANDLER_STATE_ERROR
} FotaHandlerStateType;

typedef enum
{
    FOTA_HANDLER_RESULT_NONE = 0U,
    FOTA_HANDLER_RESULT_OK,
    FOTA_HANDLER_RESULT_INVALID_PARAM,
    FOTA_HANDLER_RESULT_INVALID_STATE,
    FOTA_HANDLER_RESULT_BUSY,
    FOTA_HANDLER_RESULT_UPDATE_FAILED,
    FOTA_HANDLER_RESULT_VERIFY_FAILED,
    FOTA_HANDLER_RESULT_ACTIVATION_FAILED,
    FOTA_HANDLER_RESULT_PROVISION_FAILED
} FotaHandlerResultType;

/*********************************************************************************************************************/
/*------------------------------------------------------APIs---------------------------------------------------------*/
/*********************************************************************************************************************/

/*
 * Initializes the FOTA adapter software context and resets the underlying update core context.
 *
 * Important:
 * - This does NOT perform UCB/OTP provisioning.
 * - This does NOT arm bank swap.
 * - This does NOT erase PFLASH.
 *
 * Initial provisioning writes UCB/OTP and must be invoked explicitly through
 * FOTA_ProvisionInitialOnce() only when needed during bring-up/factory setup.
 */
void FOTA_Init(void);

/*
 * Resets adapter state and SotaUpdateCore state.
 * Use before a new RequestDownload or after abort/failure.
 * Does not perform UCB/OTP provisioning.
 */
void FOTA_ResetContext(void);

/*
 * Optional bring-up/factory wrapper.
 * Calls SotaProvision_RunInitialOnce().
 * Do not call this for every update.
 */
Std_ReturnType FOTA_ProvisionInitialOnce(void);

/* Backward-compatible alias for the same provisioning operation. */
Std_ReturnType FOTA_RunInitialProvisioningOnce(void);

/*
 * RequestDownload(0x34) wrapper.
 * DCM should pass the image length extracted from the download request/metadata.
 * Internally calls SotaUpdate_Reset() and SotaUpdate_Begin(imageLength).
 */
Std_ReturnType FOTA_StartDownload(uint32 imageLength);

/*
 * TransferData(0x36) write callout.
 * DCM must pass firmware chunk bytes only:
 *   DataPtr    = &udsRequest[2]
 *   DataLength = udsRequestLength - 2U
 *   BlockSequenceCounter = udsRequest[1]
 *
 * This adapter does not parse UDS SID, CanTp PCI, CAN ID, or lower-layer metadata.
 */
Dcm_ReturnWriteMemoryType FOTA_ProcessTransferDataWrite(
    Dcm_OpStatusType OpStatus,
    const uint8 *DataPtr,
    uint32 DataLength,
    uint8 BlockSequenceCounter
);

/*
 * Cyclic processing function.
 * Call from scheduler/main loop, not from ISR.
 * Performs actual SotaUpdate_WriteChunk() for the chunk registered by FOTA_ProcessTransferDataWrite().
 */
void FOTAHandlerMain(void);

/*
 * RequestTransferExit(0x37) / verify wrapper.
 * Internally calls SotaUpdate_FinalizeAndVerify(expectedCrc).
 */
Std_ReturnType FOTA_RequestTransferExit(uint32 expectedCrc);
Std_ReturnType FOTA_VerifyImage(uint32 expectedCrc);

/*
 * Activation / swap-arm wrapper.
 * Internally calls SotaProvision_ProgramNextSwapEntry().
 * This only arms the next UCB_SWAP entry. It does not reset the ECU.
 */
Std_ReturnType FOTA_ActivateImage(void);

/* Optional diagnostics. */
FotaHandlerStateType FOTA_GetHandlerState(void);
FotaHandlerResultType FOTA_GetLastHandlerResult(void);
FotaChunkStateType FOTA_GetChunkState(void);
uint32 FOTA_GetExpectedImageLength(void);
uint32 FOTA_GetExpectedImageCrc(void);
uint32 FOTA_GetTotalReceivedBytes(void);
uint32 FOTA_GetCurrentChunkLength(void);
uint8 FOTA_GetCurrentBlockSequenceCounter(void);
uint32 FOTA_GetLastUpdateResult(void);
uint32 FOTA_GetLastProvisionResult(void);
uint32 FOTA_GetLastSwapEntryIndex(void);
uint32 FOTA_GetLastSwapTargetModeWord(void);
Std_ReturnType FOTA_RequestSystemReset(uint32 delayTicks);
void FOTA_ResetMainFunction(void);
const void *FOTA_GetUpdateDebug(void);

#ifdef __cplusplus
}
#endif

#endif /* FOTA_HANDLER_H_ */
