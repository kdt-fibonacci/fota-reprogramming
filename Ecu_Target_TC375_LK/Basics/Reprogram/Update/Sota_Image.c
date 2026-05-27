#include "Sota_Image.h"

#include <string.h>

#define SOTA_IMAGE_CRC_INIT       (0xFFFFFFFFU)
#define SOTA_IMAGE_CRC_FINAL_XOR  (0xFFFFFFFFU)

static uint8 SotaImage_GetLocalEcuType(void);
static uint32 SotaImage_CalcCrc32(const uint8 *data, uint32 length);
static uint32 SotaImage_Crc32Update(uint32 crc, const uint8 *data, uint32 length);

SotaImage_Status_t SotaImage_ValidateManifest(const SotaImage_Manifest_t *manifest)
{
    SotaImage_Manifest_t crcManifest;
    uint32 calculatedCrc;

    if (manifest == NULL_PTR)
    {
        return SOTA_IMAGE_STATUS_INVALID_PARAM;
    }

    if (manifest->magic != SOTA_IMAGE_MANIFEST_MAGIC)
    {
        return SOTA_IMAGE_STATUS_INVALID_MAGIC;
    }

    if (manifest->protocolVersion != (uint8)SOTA_PROTOCOL_VERSION)
    {
        return SOTA_IMAGE_STATUS_UNSUPPORTED_VERSION;
    }

    if (SotaImage_IsTargetEcuMatch(manifest->ecuType) == FALSE)
    {
        return SOTA_IMAGE_STATUS_ECU_MISMATCH;
    }

    if ((manifest->imageSize == 0U) ||
        (manifest->imageSize > SotaTc37x_GetInactiveBankSize()))
    {
        return SOTA_IMAGE_STATUS_INVALID_SIZE;
    }

    (void)memcpy(&crcManifest, manifest, sizeof(SotaImage_Manifest_t));
    crcManifest.manifestCrc = 0U;
    calculatedCrc = SotaImage_CalcCrc32((const uint8 *)&crcManifest,
                                        (uint32)sizeof(SotaImage_Manifest_t));
    if (calculatedCrc != manifest->manifestCrc)
    {
        return SOTA_IMAGE_STATUS_CRC_ERROR;
    }

    return SOTA_IMAGE_STATUS_OK;
}

SotaImage_Status_t SotaImage_ValidateHeader(const SotaImage_Header_t *header)
{
    SotaImage_Header_t crcHeader;
    uint32 calculatedCrc;

    if (header == NULL_PTR)
    {
        return SOTA_IMAGE_STATUS_INVALID_PARAM;
    }

    if (header->magic != SOTA_IMAGE_HEADER_MAGIC)
    {
        return SOTA_IMAGE_STATUS_INVALID_MAGIC;
    }

    if ((header->headerVersion != (uint16)SOTA_IMAGE_HEADER_VERSION) ||
        (header->headerSize != (uint16)sizeof(SotaImage_Header_t)))
    {
        return SOTA_IMAGE_STATUS_UNSUPPORTED_VERSION;
    }

    if (SotaImage_IsTargetEcuMatch(header->ecuType) == FALSE)
    {
        return SOTA_IMAGE_STATUS_ECU_MISMATCH;
    }

    if ((header->imageSize == 0U) ||
        (header->imageSize > SotaTc37x_GetInactiveBankSize()))
    {
        return SOTA_IMAGE_STATUS_INVALID_SIZE;
    }

    (void)memcpy(&crcHeader, header, sizeof(SotaImage_Header_t));
    crcHeader.headerCrc = 0U;
    calculatedCrc = SotaImage_CalcCrc32((const uint8 *)&crcHeader,
                                        (uint32)sizeof(SotaImage_Header_t));
    if (calculatedCrc != header->headerCrc)
    {
        return SOTA_IMAGE_STATUS_CRC_ERROR;
    }

    return SOTA_IMAGE_STATUS_OK;
}

boolean SotaImage_IsTargetEcuMatch(uint8 ecuType)
{
    return (ecuType == SotaImage_GetLocalEcuType()) ? TRUE : FALSE;
}

static uint8 SotaImage_GetLocalEcuType(void)
{
#if ((SOTA_LOCAL_ECU_TYPE_MOTOR == 1U) && \
     (SOTA_LOCAL_ECU_TYPE_STEERING == 1U))
#error "Select only one SOTA local ECU type"
#elif (SOTA_LOCAL_ECU_TYPE_MOTOR == 1U)
    return (uint8)SOTA_ECU_TYPE_MOTOR;
#elif (SOTA_LOCAL_ECU_TYPE_STEERING == 1U)
    return (uint8)SOTA_ECU_TYPE_STEERING;
#else
    return (uint8)SOTA_ECU_TYPE_UNKNOWN;
#endif
}

static uint32 SotaImage_CalcCrc32(const uint8 *data, uint32 length)
{
    return SotaImage_Crc32Update(SOTA_IMAGE_CRC_INIT, data, length) ^
           SOTA_IMAGE_CRC_FINAL_XOR;
}

static uint32 SotaImage_Crc32Update(uint32 crc, const uint8 *data, uint32 length)
{
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

    return crc;
}
