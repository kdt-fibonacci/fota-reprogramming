#ifndef SOTA_METADATA_H_
#define SOTA_METADATA_H_

#include "Ifx_Types.h"

#ifndef SOTA_METADATA_DFLASH_START_ADDR
#define SOTA_METADATA_DFLASH_START_ADDR   (0xAF000000U)
#endif

#ifndef SOTA_METADATA_SLOT_COUNT
/*
 * Runtime metadata is append-only. Do not erase/compact this area during a
 * normal update; use SotaMetadata_Format() only in factory/debug flows.
 */
#define SOTA_METADATA_SLOT_COUNT          (64U)
#endif

#ifndef SOTA_METADATA_PAGE_SIZE_BYTE
#define SOTA_METADATA_PAGE_SIZE_BYTE      (8U)
#endif

#ifndef SOTA_METADATA_ERASE_SECTOR_COUNT
/*
 * TC37x DFlash logical sectors are 4 KiB. 64 metadata slots currently span
 * more than one sector, so format must erase two sectors or stale tail slots
 * can survive and be selected as the latest valid record.
 */
#define SOTA_METADATA_ERASE_SECTOR_COUNT  (2U)
#endif

#define SOTA_METADATA_MAGIC               (0x534F5441U)
#define SOTA_METADATA_VERSION             (1U)
#define SOTA_METADATA_VALID_MARKER        (0xA55A5AA5U)
#define SOTA_METADATA_ERASED_WORD         (0xFFFFFFFFU)

typedef enum
{
    SOTA_METADATA_STATUS_OK = 0,
    SOTA_METADATA_STATUS_INVALID_PARAM,
    SOTA_METADATA_STATUS_EMPTY,
    SOTA_METADATA_STATUS_NOT_FOUND,
    SOTA_METADATA_STATUS_NO_EMPTY_SLOT,
    SOTA_METADATA_STATUS_HEADER_ERROR,
    SOTA_METADATA_STATUS_CRC_ERROR,
    SOTA_METADATA_STATUS_WRITE_ERROR,
    SOTA_METADATA_STATUS_VERIFY_ERROR
} SotaMetadata_Status_t;

typedef enum
{
    SOTA_METADATA_STATE_IDLE = 0,
    SOTA_METADATA_STATE_DOWNLOAD_REQUESTED,
    SOTA_METADATA_STATE_ERASING,
    SOTA_METADATA_STATE_RECEIVING,
    SOTA_METADATA_STATE_PROGRAMMING,
    SOTA_METADATA_STATE_VERIFYING,
    SOTA_METADATA_STATE_VERIFIED,
    SOTA_METADATA_STATE_SWAP_ARMED,
    SOTA_METADATA_STATE_TESTING_AFTER_BOOT,
    SOTA_METADATA_STATE_COMMITTED,
    SOTA_METADATA_STATE_ROLLBACK_ARMED,
    SOTA_METADATA_STATE_ROLLED_BACK,
    SOTA_METADATA_STATE_ERROR,
    SOTA_METADATA_STATE_SWAP_ARM_REQUESTED,
    SOTA_METADATA_STATE_ROLLBACK_REQUESTED
} SotaMetadata_State_t;

typedef struct
{
    uint32 magic;
    uint16 version;
    uint16 recordSize;
    uint32 sequence;
    SotaMetadata_State_t state;
    uint16 ecuType;
    uint8  activeBankBeforeUpdate;
    uint8  targetBank;
    uint8  currentActiveBank;
    uint8  reserved0[3];
    uint32 imageVersion;
    uint32 imageSize;
    uint32 imageCrc;
    uint32 manifestCrc;
    uint32 receivedBytes;
    uint32 programmedBytes;
    uint32 verifiedBytes;
    uint32 lastSeq;
    uint32 lastOffset;
    uint16 swapEntryIndex;
    uint8  bootAttempt;
    uint8  maxBootAttempt;
    uint32 lastError;
    uint32 rollbackReason;
    uint32 payloadCrc;
    uint32 validMarker;
} SotaMetadata_Record_t;

/*
 * Metadata write APIs must be called from background/runtime context.
 * Do not call them from CAN-FD ISR or any interrupt context.
 */
SotaMetadata_Status_t SotaMetadata_Init(void);
SotaMetadata_Status_t SotaMetadata_LoadLatest(SotaMetadata_Record_t *outRecord);
SotaMetadata_Status_t SotaMetadata_Append(const SotaMetadata_Record_t *record);
SotaMetadata_Status_t SotaMetadata_UpdateState(SotaMetadata_State_t state);
SotaMetadata_Status_t SotaMetadata_UpdateProgress(uint32 receivedBytes,
                                                  uint32 programmedBytes,
                                                  uint32 verifiedBytes,
                                                  uint32 lastSeq,
                                                  uint32 lastOffset);
SotaMetadata_Status_t SotaMetadata_SetError(uint32 lastError);
SotaMetadata_Status_t SotaMetadata_SetRollbackReason(uint32 rollbackReason);
SotaMetadata_Status_t SotaMetadata_Format(void);

uint32 SotaMetadata_GetSlotCount(void);
uint32 SotaMetadata_GetSlotSizeByte(void);

#endif /* SOTA_METADATA_H_ */
