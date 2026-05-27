#ifndef SOTA_UPDATECORE_H_
#define SOTA_UPDATECORE_H_

#include "Ifx_Types.h"

typedef enum
{
    SOTA_UPDATECORE_RESULT_OK = 0,
    SOTA_UPDATECORE_RESULT_PENDING,
    SOTA_UPDATECORE_RESULT_INVALID_STATE,
    SOTA_UPDATECORE_RESULT_INVALID_PARAM,
    SOTA_UPDATECORE_RESULT_INVALID_RANGE,
    SOTA_UPDATECORE_RESULT_CRC_ERROR,
    SOTA_UPDATECORE_RESULT_FLASH_ERROR,
    SOTA_UPDATECORE_RESULT_VERIFY_ERROR,
    SOTA_UPDATECORE_RESULT_INTERNAL_ERROR
} SotaUpdateCore_Result_t;

typedef enum
{
    SOTA_UPDATECORE_STATE_IDLE = 0,
    SOTA_UPDATECORE_STATE_PREPARED,
    SOTA_UPDATECORE_STATE_ERASING,
    SOTA_UPDATECORE_STATE_ERASED,
    SOTA_UPDATECORE_STATE_WRITING,
    SOTA_UPDATECORE_STATE_FINALIZED,
    SOTA_UPDATECORE_STATE_VERIFYING,
    SOTA_UPDATECORE_STATE_VERIFIED,
    SOTA_UPDATECORE_STATE_ERROR
} SotaUpdateCore_State_t;

typedef struct
{
    SotaUpdateCore_State_t state;
    uint8 targetBank;
    uint32 inactiveBankBase;
    uint32 inactiveBankSize;
    uint32 imageSize;
    uint32 expectedImageCrc;
    uint32 eraseBytes;
    uint32 erasedBytes;
    uint32 receivedBytes;
    uint32 programmedBytes;
    uint32 verifiedBytes;
    uint32 lastError;
} SotaUpdateCore_Progress_t;

/*
 * These APIs perform or schedule flash-sensitive work. Call them only from
 * background/runtime context such as SotaUpdateManager_RunStep(), never from a
 * CAN-FD RX ISR path.
 */
SotaUpdateCore_Result_t SotaUpdateCore_Prepare(uint32 imageSize,
                                               uint32 expectedImageCrc);
SotaUpdateCore_Result_t SotaUpdateCore_EraseStep(void);
SotaUpdateCore_Result_t SotaUpdateCore_WriteChunk(uint32 offset,
                                                  const uint8 *data,
                                                  uint32 length,
                                                  uint32 chunkCrc);
SotaUpdateCore_Result_t SotaUpdateCore_VerifyStep(void);
SotaUpdateCore_Result_t SotaUpdateCore_Finalize(void);
SotaUpdateCore_Result_t SotaUpdateCore_Abort(void);
void SotaUpdateCore_GetProgress(SotaUpdateCore_Progress_t *outProgress);

#endif /* SOTA_UPDATECORE_H_ */
