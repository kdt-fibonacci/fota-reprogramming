#ifndef FOTA_HANDLER_H_
#define FOTA_HANDLER_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

/*
 * Expected include paths when this file is placed under Basics/Reprogram/:
 * - Basics/ComStack/Dcm is on the compiler include path, so "Dcm.h" resolves.
 * - Basics/Reprogram is on the compiler include path, so "Update/Sota_UpdateCore.h" resolves.
 *
 * If your project only exposes the original Sota include root, replace the second
 * include with <Sota/Update/Sota_UpdateCore.h> or add Basics/Reprogram to the
 * include path.
 */
#include "Dcm.h"
#include "Update/Sota_UpdateCore.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

#ifndef FOTA_MAX_TRANSFER_DATA_LENGTH
#define FOTA_MAX_TRANSFER_DATA_LENGTH    (512U)
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

/*********************************************************************************************************************/
/*------------------------------------------------------APIs---------------------------------------------------------*/
/*********************************************************************************************************************/

/*
 * DCM TransferData(0x36) write callout.
 *
 * DCM must pass only firmware chunk bytes through DataPtr/DataLength.
 * UDS SID(0x36) and blockSequenceCounter must be stripped before this call.
 * BlockSequenceCounter is stored for diagnostics but is not interpreted as
 * CanTp consecutive-frame sequence number.
 *
 * Call pattern:
 * 1) DCM_OP_INITIAL with a new chunk -> registers the chunk and returns DCM_WRITE_PENDING.
 * 2) FOTAHandlerMain() runs periodically and calls SotaUpdate_WriteChunk().
 * 3) DCM_OP_PENDING -> returns DCM_WRITE_OK after the handler finishes, or
 *    DCM_WRITE_FAILED if the handler hit an error.
 */
Dcm_ReturnWriteMemoryType FOTA_ProcessTransferDataWrite(
    Dcm_OpStatusType OpStatus,
    const uint8 *DataPtr,
    uint32 DataLength,
    uint8 BlockSequenceCounter
);

/*
 * Cyclic FOTA handler.
 * Call from the application scheduler/main loop, not from an ISR.
 */
void FOTAHandlerMain(void);

/*
 * Reset only the adapter/chunk state. This does not erase flash and does not
 * call SotaUpdate_Reset(). Use it when DCM starts a fresh RequestDownload or
 * aborts a pending TransferData operation.
 */
void FOTA_ResetContext(void);

/* Optional diagnostics for DCM/app debug. */
FotaChunkStateType FOTA_GetChunkState(void);
uint32 FOTA_GetTotalReceivedBytes(void);
uint32 FOTA_GetCurrentChunkLength(void);
uint8 FOTA_GetCurrentBlockSequenceCounter(void);
SotaUpdateResult_t FOTA_GetLastSotaResult(void);

#ifdef __cplusplus
}
#endif

#endif /* FOTA_HANDLER_H_ */
