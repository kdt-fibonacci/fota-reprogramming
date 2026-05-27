#ifndef SOTA_UPDATE_CORE_H
#define SOTA_UPDATE_CORE_H 1

#include <Ifx_Types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SOTA_UPDATE_STATE_IDLE = 0,
    SOTA_UPDATE_STATE_ERASING,
    SOTA_UPDATE_STATE_RECEIVING,
    SOTA_UPDATE_STATE_PROGRAMMING,
    SOTA_UPDATE_STATE_VERIFYING,
    SOTA_UPDATE_STATE_VERIFIED,
    SOTA_UPDATE_STATE_ERROR
} SotaUpdateState_t;

typedef enum
{
    SOTA_UPDATE_OK = 0,
    SOTA_UPDATE_INVALID_PARAM,
    SOTA_UPDATE_INVALID_LENGTH,
    SOTA_UPDATE_INVALID_BANK,
    SOTA_UPDATE_ERASE_FAILED,
    SOTA_UPDATE_PROGRAM_FAILED,
    SOTA_UPDATE_VERIFY_FAILED,
    SOTA_UPDATE_CRC_FAILED,
    SOTA_UPDATE_RESULT_STATE_ERROR
} SotaUpdateResult_t;

typedef struct
{
    uint32 inactiveBase;
    uint32 inactiveEnd;

    uint32 imageLength;
    uint32 expectedCrc;
    uint32 actualCrc;

    uint32 paddedImageLength;
    uint32 sectorCount;
    uint32 eraseSize;
    uint32 eraseStart;
    uint32 eraseEnd;

    uint32 receivedBytes;
    uint32 programmedBytes;
    uint32 currentPageFill;

    uint32 eraseResult;
    uint32 programFailOffset;
    uint32 verifyFailOffset;
    uint32 finalizeResult;

    uint32 dmuErrAfterErase;
    uint32 dmuErrAfterProgram;

    uint32 state;
    uint32 done;
} SotaUpdateDebug_t;

void SotaUpdate_Reset(void);
SotaUpdateResult_t SotaUpdate_Begin(uint32 imageLength);
SotaUpdateResult_t SotaUpdate_WriteChunk(const uint8 *data, uint32 len);
SotaUpdateResult_t SotaUpdate_FinalizeAndVerify(uint32 expectedCrc);
SotaUpdateState_t SotaUpdate_GetState(void);
const SotaUpdateDebug_t *SotaUpdate_GetDebug(void);

#ifdef __cplusplus
}
#endif

#endif /* SOTA_UPDATE_CORE_H */
