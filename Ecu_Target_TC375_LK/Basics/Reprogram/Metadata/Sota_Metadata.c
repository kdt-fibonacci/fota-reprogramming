#include "Sota_Metadata.h"

#include <stddef.h>
#include <string.h>

#include "IfxFlash.h"
#include "IfxScuWdt.h"
#include "Cpu/Std/IfxCpu.h"

#if defined(__GNUC__)
#define SOTA_METADATA_RAM_CODE __attribute__((section(".cpu0_psram")))
#else
#define SOTA_METADATA_RAM_CODE
#endif

#define SOTA_METADATA_ALIGN_UP(value, align) \
    ((((uint32)(value)) + ((uint32)(align) - 1U)) / (uint32)(align) * (uint32)(align))

#define SOTA_METADATA_RECORD_SIZE_BYTE \
    ((uint32)sizeof(SotaMetadata_Record_t))

#define SOTA_METADATA_SLOT_SIZE_BYTE \
    (SOTA_METADATA_ALIGN_UP(SOTA_METADATA_RECORD_SIZE_BYTE, SOTA_METADATA_PAGE_SIZE_BYTE))

#define SOTA_METADATA_SLOT_PAGE_COUNT \
    (SOTA_METADATA_SLOT_SIZE_BYTE / SOTA_METADATA_PAGE_SIZE_BYTE)

#define SOTA_METADATA_PAYLOAD_CRC_OFFSET \
    ((uint32)offsetof(SotaMetadata_Record_t, payloadCrc))

#define SOTA_METADATA_VALID_MARKER_OFFSET \
    ((uint32)offsetof(SotaMetadata_Record_t, validMarker))

#define SOTA_METADATA_DFLASH_LOG_SECTOR_SIZE_BYTE (0x1000U)
#define SOTA_METADATA_FORMAT_ERASE_SIZE_BYTE \
    (SOTA_METADATA_ERASE_SECTOR_COUNT * SOTA_METADATA_DFLASH_LOG_SECTOR_SIZE_BYTE)

static uint32 SotaMetadata_CalcCrc32(const uint8 *data, uint32 length);
static uint32 SotaMetadata_GetSlotAddress(uint32 slotIndex);
static boolean SotaMetadata_IsSequenceNewer(uint32 candidate, uint32 current);
static boolean SotaMetadata_IsBlankBytes(const uint8 *data, uint32 length);
static boolean SotaMetadata_IsBlankRange(uint32 startAddress, uint32 length);
static boolean SotaMetadata_IsBlankRecord(const SotaMetadata_Record_t *record);
static void SotaMetadata_NormalizeForWrite(SotaMetadata_Record_t *record, uint32 sequence);
static SotaMetadata_Status_t SotaMetadata_ReadRawSlot(uint32 slotIndex,
                                                      SotaMetadata_Record_t *outRecord);
static SotaMetadata_Status_t SotaMetadata_ValidateSlot(uint32 slotIndex,
                                                       SotaMetadata_Record_t *outRecord);
static SotaMetadata_Status_t SotaMetadata_FindLatestValidSlot(uint32 *outSlot,
                                                              SotaMetadata_Record_t *outRecord);
static SotaMetadata_Status_t SotaMetadata_FindFirstEmptySlot(uint32 *outSlot);
static SotaMetadata_Status_t SOTA_METADATA_RAM_CODE SotaMetadata_WritePage(uint32 pageAddr,
                                                                           const uint8 *pageData);
static SotaMetadata_Status_t SotaMetadata_WriteSlot(uint32 slotIndex,
                                                    const SotaMetadata_Record_t *record);
static SotaMetadata_Status_t SotaMetadata_LoadLatestOrDefault(SotaMetadata_Record_t *record);
static void SotaMetadata_SetDefaultRecord(SotaMetadata_Record_t *record);

volatile uint32 g_sotaMetadataDebugFormatStartAddr;
volatile uint32 g_sotaMetadataDebugFormatEraseBytes;
volatile uint32 g_sotaMetadataDebugFormatMetadataBytes;
volatile uint32 g_sotaMetadataDebugFormatStatus;
volatile uint32 g_sotaMetadataDebugFormatDmuErr;
volatile uint32 g_sotaMetadataDebugFormatVerifyAddr;

SotaMetadata_Status_t SotaMetadata_Init(void)
{
    uint32 latestSlot;
    SotaMetadata_Record_t latestRecord;
    SotaMetadata_Status_t status;

    status = SotaMetadata_FindLatestValidSlot(&latestSlot, &latestRecord);
    (void)latestSlot;

    if (status == SOTA_METADATA_STATUS_OK)
    {
        (void)latestRecord;
        return SOTA_METADATA_STATUS_OK;
    }

    if ((status == SOTA_METADATA_STATUS_EMPTY) ||
        (status == SOTA_METADATA_STATUS_NOT_FOUND))
    {
        return status;
    }

    return status;
}

SotaMetadata_Status_t SotaMetadata_LoadLatest(SotaMetadata_Record_t *outRecord)
{
    uint32 latestSlot;
    SotaMetadata_Record_t latestRecord;
    SotaMetadata_Status_t status;

    if (outRecord == NULL_PTR)
    {
        return SOTA_METADATA_STATUS_INVALID_PARAM;
    }

    status = SotaMetadata_FindLatestValidSlot(&latestSlot, &latestRecord);
    (void)latestSlot;

    if (status == SOTA_METADATA_STATUS_OK)
    {
        *outRecord = latestRecord;
        return SOTA_METADATA_STATUS_OK;
    }

    return status;
}

SotaMetadata_Status_t SotaMetadata_Append(const SotaMetadata_Record_t *record)
{
    uint32 latestSlot;
    uint32 targetSlot;
    uint32 nextSequence = 0U;
    SotaMetadata_Record_t latestRecord;
    SotaMetadata_Record_t writeRecord;
    SotaMetadata_Status_t status;

    /*
     * Power-loss behavior:
     * A complete record is append-only. Body fields and payloadCrc are
     * programmed first with validMarker erased. validMarker is programmed in
     * the final flash write. Boot scan ignores records without validMarker.
     */
    if (record == NULL_PTR)
    {
        return SOTA_METADATA_STATUS_INVALID_PARAM;
    }

    status = SotaMetadata_FindLatestValidSlot(&latestSlot, &latestRecord);
    (void)latestSlot;

    if (status == SOTA_METADATA_STATUS_OK)
    {
        nextSequence = latestRecord.sequence + 1U;
    }
    else if ((status == SOTA_METADATA_STATUS_EMPTY) ||
             (status == SOTA_METADATA_STATUS_NOT_FOUND))
    {
        nextSequence = 0U;
    }
    else
    {
        return status;
    }

    status = SotaMetadata_FindFirstEmptySlot(&targetSlot);
    if (status != SOTA_METADATA_STATUS_OK)
    {
        return SOTA_METADATA_STATUS_NO_EMPTY_SLOT;
    }

    writeRecord = *record;
    SotaMetadata_NormalizeForWrite(&writeRecord, nextSequence);

    return SotaMetadata_WriteSlot(targetSlot, &writeRecord);
}

SotaMetadata_Status_t SotaMetadata_UpdateState(SotaMetadata_State_t state)
{
    SotaMetadata_Record_t record;
    SotaMetadata_Status_t status;

    status = SotaMetadata_LoadLatestOrDefault(&record);
    if (status != SOTA_METADATA_STATUS_OK)
    {
        return status;
    }

    record.state = state;
    return SotaMetadata_Append(&record);
}

SotaMetadata_Status_t SotaMetadata_UpdateProgress(uint32 receivedBytes,
                                                  uint32 programmedBytes,
                                                  uint32 verifiedBytes,
                                                  uint32 lastSeq,
                                                  uint32 lastOffset)
{
    SotaMetadata_Record_t record;
    SotaMetadata_Status_t status;

    status = SotaMetadata_LoadLatest(&record);
    if (status != SOTA_METADATA_STATUS_OK)
    {
        return status;
    }

    record.receivedBytes = receivedBytes;
    record.programmedBytes = programmedBytes;
    record.verifiedBytes = verifiedBytes;
    record.lastSeq = lastSeq;
    record.lastOffset = lastOffset;

    return SotaMetadata_Append(&record);
}

SotaMetadata_Status_t SotaMetadata_SetError(uint32 lastError)
{
    SotaMetadata_Record_t record;
    SotaMetadata_Status_t status;

    status = SotaMetadata_LoadLatestOrDefault(&record);
    if (status != SOTA_METADATA_STATUS_OK)
    {
        return status;
    }

    record.state = SOTA_METADATA_STATE_ERROR;
    record.lastError = lastError;

    return SotaMetadata_Append(&record);
}

SotaMetadata_Status_t SotaMetadata_SetRollbackReason(uint32 rollbackReason)
{
    SotaMetadata_Record_t record;
    SotaMetadata_Status_t status;

    status = SotaMetadata_LoadLatestOrDefault(&record);
    if (status != SOTA_METADATA_STATUS_OK)
    {
        return status;
    }

    record.rollbackReason = rollbackReason;

    return SotaMetadata_Append(&record);
}

SotaMetadata_Status_t SotaMetadata_Format(void)
{
    uint16 endInitPassword;
    boolean interruptState;
    uint32 metadataBytes =
        SOTA_METADATA_SLOT_COUNT * SOTA_METADATA_SLOT_SIZE_BYTE;

    /*
     * Factory/debug-style format. Runtime update flow should append records;
     * format erases the metadata sector and resets recovery history.
     */
    g_sotaMetadataDebugFormatStartAddr = SOTA_METADATA_DFLASH_START_ADDR;
    g_sotaMetadataDebugFormatEraseBytes = SOTA_METADATA_FORMAT_ERASE_SIZE_BYTE;
    g_sotaMetadataDebugFormatMetadataBytes = metadataBytes;
    g_sotaMetadataDebugFormatStatus = SOTA_METADATA_STATUS_WRITE_ERROR;
    g_sotaMetadataDebugFormatDmuErr = 0U;
    g_sotaMetadataDebugFormatVerifyAddr = 0U;

    if (SOTA_METADATA_FORMAT_ERASE_SIZE_BYTE < metadataBytes)
    {
        g_sotaMetadataDebugFormatStatus = SOTA_METADATA_STATUS_INVALID_PARAM;
        return SOTA_METADATA_STATUS_INVALID_PARAM;
    }

    if (IfxFlash_waitUnbusy(0U, IfxFlash_FlashType_D0) != 0U)
    {
        g_sotaMetadataDebugFormatStatus = SOTA_METADATA_STATUS_WRITE_ERROR;
        return SOTA_METADATA_STATUS_WRITE_ERROR;
    }

    IfxFlash_clearStatus(0U);
    endInitPassword = IfxScuWdt_getSafetyWatchdogPassword();

    interruptState = IfxCpu_disableInterrupts();
    IfxScuWdt_clearSafetyEndinit(endInitPassword);
    IfxFlash_eraseMultipleSectors(SOTA_METADATA_DFLASH_START_ADDR,
                                  SOTA_METADATA_ERASE_SECTOR_COUNT);
    IfxScuWdt_setSafetyEndinit(endInitPassword);
    IfxCpu_restoreInterrupts(interruptState);

    if (IfxFlash_waitUnbusy(0U, IfxFlash_FlashType_D0) != 0U)
    {
        g_sotaMetadataDebugFormatStatus = SOTA_METADATA_STATUS_WRITE_ERROR;
        return SOTA_METADATA_STATUS_WRITE_ERROR;
    }

    g_sotaMetadataDebugFormatDmuErr = DMU_HF_ERRSR.U;
    if (g_sotaMetadataDebugFormatDmuErr != 0U)
    {
        g_sotaMetadataDebugFormatStatus = SOTA_METADATA_STATUS_WRITE_ERROR;
        return SOTA_METADATA_STATUS_WRITE_ERROR;
    }

    if (SotaMetadata_IsBlankRange(SOTA_METADATA_DFLASH_START_ADDR,
                                  metadataBytes) == FALSE)
    {
        g_sotaMetadataDebugFormatStatus = SOTA_METADATA_STATUS_VERIFY_ERROR;
        return SOTA_METADATA_STATUS_VERIFY_ERROR;
    }

    g_sotaMetadataDebugFormatStatus = SOTA_METADATA_STATUS_OK;
    return SOTA_METADATA_STATUS_OK;
}

uint32 SotaMetadata_GetSlotCount(void)
{
    return SOTA_METADATA_SLOT_COUNT;
}

uint32 SotaMetadata_GetSlotSizeByte(void)
{
    return SOTA_METADATA_SLOT_SIZE_BYTE;
}

static uint32 SotaMetadata_CalcCrc32(const uint8 *data, uint32 length)
{
    uint32 crc = 0xFFFFFFFFU;
    uint32 i;
    uint32 bit;

    if (data == NULL_PTR)
    {
        return 0U;
    }

    for (i = 0U; i < length; i++)
    {
        crc ^= (uint32)data[i];

        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1U) ^ 0xEDB88320U;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc ^ 0xFFFFFFFFU;
}

static uint32 SotaMetadata_GetSlotAddress(uint32 slotIndex)
{
    return SOTA_METADATA_DFLASH_START_ADDR +
           (slotIndex * SOTA_METADATA_SLOT_SIZE_BYTE);
}

static boolean SotaMetadata_IsSequenceNewer(uint32 candidate, uint32 current)
{
    return (candidate > current) ? TRUE : FALSE;
}

static boolean SotaMetadata_IsBlankBytes(const uint8 *data, uint32 length)
{
    uint32 i;
    boolean allFF = TRUE;
    boolean all00 = TRUE;

    if (data == NULL_PTR)
    {
        return FALSE;
    }

    for (i = 0U; i < length; i++)
    {
        if (data[i] != 0xFFU)
        {
            allFF = FALSE;
        }

        if (data[i] != 0x00U)
        {
            all00 = FALSE;
        }
    }

    return ((allFF == TRUE) || (all00 == TRUE)) ? TRUE : FALSE;
}

static boolean SotaMetadata_IsBlankRange(uint32 startAddress, uint32 length)
{
    uint32 offset;
    const volatile uint8 *data = (const volatile uint8 *)startAddress;

    for (offset = 0U; offset < length; offset++)
    {
        if (data[offset] != 0xFFU)
        {
            g_sotaMetadataDebugFormatVerifyAddr = startAddress + offset;
            return FALSE;
        }
    }

    return TRUE;
}

static boolean SotaMetadata_IsBlankRecord(const SotaMetadata_Record_t *record)
{
    if (record == NULL_PTR)
    {
        return FALSE;
    }

    return SotaMetadata_IsBlankBytes((const uint8 *)record,
                                     SOTA_METADATA_RECORD_SIZE_BYTE);
}

static void SotaMetadata_NormalizeForWrite(SotaMetadata_Record_t *record,
                                           uint32 sequence)
{
    if (record != NULL_PTR)
    {
        record->magic = SOTA_METADATA_MAGIC;
        record->version = (uint16)SOTA_METADATA_VERSION;
        record->recordSize = (uint16)SOTA_METADATA_RECORD_SIZE_BYTE;
        record->sequence = sequence;
        record->validMarker = SOTA_METADATA_ERASED_WORD;
        record->payloadCrc = SotaMetadata_CalcCrc32((const uint8 *)record,
                                                    SOTA_METADATA_PAYLOAD_CRC_OFFSET);
        record->validMarker = SOTA_METADATA_VALID_MARKER;
    }
}

static SotaMetadata_Status_t SotaMetadata_ReadRawSlot(uint32 slotIndex,
                                                      SotaMetadata_Record_t *outRecord)
{
    const void *slotPtr;

    if ((slotIndex >= SOTA_METADATA_SLOT_COUNT) || (outRecord == NULL_PTR))
    {
        return SOTA_METADATA_STATUS_INVALID_PARAM;
    }

    slotPtr = (const void *)SotaMetadata_GetSlotAddress(slotIndex);
    (void)memcpy(outRecord, slotPtr, sizeof(SotaMetadata_Record_t));

    if (SotaMetadata_IsBlankRecord(outRecord) == TRUE)
    {
        return SOTA_METADATA_STATUS_EMPTY;
    }

    return SOTA_METADATA_STATUS_OK;
}

static SotaMetadata_Status_t SotaMetadata_ValidateSlot(uint32 slotIndex,
                                                       SotaMetadata_Record_t *outRecord)
{
    SotaMetadata_Record_t record;
    uint32 expectedCrc;
    SotaMetadata_Status_t status;

    if (outRecord == NULL_PTR)
    {
        return SOTA_METADATA_STATUS_INVALID_PARAM;
    }

    status = SotaMetadata_ReadRawSlot(slotIndex, &record);
    if (status != SOTA_METADATA_STATUS_OK)
    {
        return status;
    }

    if ((record.magic != SOTA_METADATA_MAGIC) ||
        (record.version != (uint16)SOTA_METADATA_VERSION) ||
        (record.recordSize != (uint16)SOTA_METADATA_RECORD_SIZE_BYTE) ||
        (record.validMarker != SOTA_METADATA_VALID_MARKER))
    {
        return SOTA_METADATA_STATUS_HEADER_ERROR;
    }

    expectedCrc = SotaMetadata_CalcCrc32((const uint8 *)&record,
                                         SOTA_METADATA_PAYLOAD_CRC_OFFSET);

    if (record.payloadCrc != expectedCrc)
    {
        return SOTA_METADATA_STATUS_CRC_ERROR;
    }

    *outRecord = record;
    return SOTA_METADATA_STATUS_OK;
}

static SotaMetadata_Status_t SotaMetadata_FindLatestValidSlot(uint32 *outSlot,
                                                              SotaMetadata_Record_t *outRecord)
{
    uint32 slot;
    uint32 bestSlot = 0U;
    boolean foundValid = FALSE;
    boolean sawEmpty = FALSE;
    SotaMetadata_Record_t bestRecord;
    SotaMetadata_Record_t tempRecord;
    SotaMetadata_Status_t status;

    if ((outSlot == NULL_PTR) || (outRecord == NULL_PTR))
    {
        return SOTA_METADATA_STATUS_INVALID_PARAM;
    }

    for (slot = 0U; slot < SOTA_METADATA_SLOT_COUNT; slot++)
    {
        status = SotaMetadata_ValidateSlot(slot, &tempRecord);

        if (status == SOTA_METADATA_STATUS_OK)
        {
            if ((foundValid == FALSE) ||
                (SotaMetadata_IsSequenceNewer(tempRecord.sequence,
                                              bestRecord.sequence) == TRUE))
            {
                foundValid = TRUE;
                bestSlot = slot;
                bestRecord = tempRecord;
            }
        }
        else if (status == SOTA_METADATA_STATUS_EMPTY)
        {
            sawEmpty = TRUE;
        }
        else
        {
            /* Corrupted or partially written slot: ignore and keep scanning. */
        }
    }

    if (foundValid == TRUE)
    {
        *outSlot = bestSlot;
        *outRecord = bestRecord;
        return SOTA_METADATA_STATUS_OK;
    }

    return (sawEmpty == TRUE) ? SOTA_METADATA_STATUS_EMPTY
                              : SOTA_METADATA_STATUS_NOT_FOUND;
}

static SotaMetadata_Status_t SotaMetadata_FindFirstEmptySlot(uint32 *outSlot)
{
    uint32 slot;
    SotaMetadata_Record_t record;
    SotaMetadata_Status_t status;

    if (outSlot == NULL_PTR)
    {
        return SOTA_METADATA_STATUS_INVALID_PARAM;
    }

    for (slot = 0U; slot < SOTA_METADATA_SLOT_COUNT; slot++)
    {
        status = SotaMetadata_ReadRawSlot(slot, &record);
        if (status == SOTA_METADATA_STATUS_EMPTY)
        {
            *outSlot = slot;
            return SOTA_METADATA_STATUS_OK;
        }
    }

    return SOTA_METADATA_STATUS_NO_EMPTY_SLOT;
}

#if defined(__TASKING__)
#pragma section code "cpu0_psram"
#endif

static SotaMetadata_Status_t SOTA_METADATA_RAM_CODE SotaMetadata_WritePage(uint32 pageAddr,
                                                                           const uint8 *pageData)
{
    uint32 wordL;
    uint32 wordU;
    uint16 endInitPassword;
    boolean interruptState;

    if (pageData == NULL_PTR)
    {
        return SOTA_METADATA_STATUS_INVALID_PARAM;
    }

    (void)memcpy(&wordL, &pageData[0], sizeof(uint32));
    (void)memcpy(&wordU, &pageData[sizeof(uint32)], sizeof(uint32));

    interruptState = IfxCpu_disableInterrupts();

    if (IfxFlash_enterPageMode(pageAddr) != 0U)
    {
        IfxCpu_restoreInterrupts(interruptState);
        return SOTA_METADATA_STATUS_WRITE_ERROR;
    }

    if (IfxFlash_waitUnbusy(0U, IfxFlash_FlashType_D0) != 0U)
    {
        IfxCpu_restoreInterrupts(interruptState);
        return SOTA_METADATA_STATUS_WRITE_ERROR;
    }

    IfxFlash_loadPage2X32(pageAddr, wordL, wordU);

    endInitPassword = IfxScuWdt_getSafetyWatchdogPasswordInline();
    IfxScuWdt_clearSafetyEndinitInline(endInitPassword);
    IfxFlash_writePage(pageAddr);
    IfxScuWdt_setSafetyEndinitInline(endInitPassword);

    if (IfxFlash_waitUnbusy(0U, IfxFlash_FlashType_D0) != 0U)
    {
        IfxCpu_restoreInterrupts(interruptState);
        return SOTA_METADATA_STATUS_WRITE_ERROR;
    }

    IfxCpu_restoreInterrupts(interruptState);

    return SOTA_METADATA_STATUS_OK;
}

#if defined(__TASKING__)
#pragma section code restore
#endif

static SotaMetadata_Status_t SotaMetadata_WriteSlot(uint32 slotIndex,
                                                    const SotaMetadata_Record_t *record)
{
    uint8 buffer[SOTA_METADATA_SLOT_SIZE_BYTE];
    uint8 markerPage[SOTA_METADATA_PAGE_SIZE_BYTE];
    uint32 page;
    uint32 pageAddr;
    uint32 slotAddr;
    uint32 markerPageIndex;
    uint32 markerPageOffset;
    uint32 markerOffsetInPage;
    uint32 markerValue = SOTA_METADATA_VALID_MARKER;
    SotaMetadata_Record_t bodyRecord;
    SotaMetadata_Record_t verifyRecord;
    SotaMetadata_Status_t status;

    if ((slotIndex >= SOTA_METADATA_SLOT_COUNT) || (record == NULL_PTR))
    {
        return SOTA_METADATA_STATUS_INVALID_PARAM;
    }

    slotAddr = SotaMetadata_GetSlotAddress(slotIndex);

    (void)memset(buffer, 0xFF, sizeof(buffer));

    bodyRecord = *record;
    bodyRecord.validMarker = SOTA_METADATA_ERASED_WORD;
    (void)memcpy(buffer, &bodyRecord, sizeof(SotaMetadata_Record_t));

    markerPageIndex =
        SOTA_METADATA_VALID_MARKER_OFFSET / SOTA_METADATA_PAGE_SIZE_BYTE;
    markerPageOffset = markerPageIndex * SOTA_METADATA_PAGE_SIZE_BYTE;
    markerOffsetInPage = SOTA_METADATA_VALID_MARKER_OFFSET - markerPageOffset;

    for (page = 0U; page < SOTA_METADATA_SLOT_PAGE_COUNT; page++)
    {
        if (page == markerPageIndex)
        {
            continue;
        }

        pageAddr = slotAddr + (page * SOTA_METADATA_PAGE_SIZE_BYTE);
        status = SotaMetadata_WritePage(pageAddr,
                                        &buffer[page * SOTA_METADATA_PAGE_SIZE_BYTE]);
        if (status != SOTA_METADATA_STATUS_OK)
        {
            return SOTA_METADATA_STATUS_WRITE_ERROR;
        }
    }

    (void)memcpy(markerPage, &buffer[markerPageOffset], sizeof(markerPage));
    (void)memcpy(&markerPage[markerOffsetInPage], &markerValue, sizeof(markerValue));

    status = SotaMetadata_WritePage(slotAddr + markerPageOffset, markerPage);
    if (status != SOTA_METADATA_STATUS_OK)
    {
        return SOTA_METADATA_STATUS_WRITE_ERROR;
    }

    status = SotaMetadata_ValidateSlot(slotIndex, &verifyRecord);
    if (status != SOTA_METADATA_STATUS_OK)
    {
        return SOTA_METADATA_STATUS_VERIFY_ERROR;
    }

    if (memcmp(&verifyRecord, record, sizeof(SotaMetadata_Record_t)) != 0)
    {
        return SOTA_METADATA_STATUS_VERIFY_ERROR;
    }

    return SOTA_METADATA_STATUS_OK;
}

static SotaMetadata_Status_t SotaMetadata_LoadLatestOrDefault(SotaMetadata_Record_t *record)
{
    SotaMetadata_Status_t status;

    if (record == NULL_PTR)
    {
        return SOTA_METADATA_STATUS_INVALID_PARAM;
    }

    status = SotaMetadata_LoadLatest(record);
    if (status == SOTA_METADATA_STATUS_OK)
    {
        return SOTA_METADATA_STATUS_OK;
    }

    if ((status == SOTA_METADATA_STATUS_EMPTY) ||
        (status == SOTA_METADATA_STATUS_NOT_FOUND))
    {
        SotaMetadata_SetDefaultRecord(record);
        return SOTA_METADATA_STATUS_OK;
    }

    return status;
}

static void SotaMetadata_SetDefaultRecord(SotaMetadata_Record_t *record)
{
    if (record != NULL_PTR)
    {
        (void)memset(record, 0, sizeof(SotaMetadata_Record_t));
        record->state = SOTA_METADATA_STATE_IDLE;
        record->maxBootAttempt = 1U;
        record->swapEntryIndex = 0xFFFFU;
        record->activeBankBeforeUpdate = 0xFFU;
        record->targetBank = 0xFFU;
        record->currentActiveBank = 0xFFU;
    }
}
