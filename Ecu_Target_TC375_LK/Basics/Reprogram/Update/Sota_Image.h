#ifndef SOTA_IMAGE_H_
#define SOTA_IMAGE_H_

#include "Ifx_Types.h"

#include "../Config/Sota_Tc37x_Config.h"
#include "../Transport/Sota_Protocol.h"

#define SOTA_IMAGE_MANIFEST_MAGIC        (0x534F544DU)
#define SOTA_IMAGE_HEADER_MAGIC          (0x534F5448U)
#define SOTA_IMAGE_HEADER_VERSION        (1U)

#ifndef SOTA_LOCAL_ECU_TYPE_MOTOR
#define SOTA_LOCAL_ECU_TYPE_MOTOR        (0U)
#endif

#ifndef SOTA_LOCAL_ECU_TYPE_STEERING
#define SOTA_LOCAL_ECU_TYPE_STEERING     (0U)
#endif

typedef enum
{
    SOTA_IMAGE_STATUS_OK = 0,
    SOTA_IMAGE_STATUS_INVALID_PARAM,
    SOTA_IMAGE_STATUS_INVALID_MAGIC,
    SOTA_IMAGE_STATUS_UNSUPPORTED_VERSION,
    SOTA_IMAGE_STATUS_ECU_MISMATCH,
    SOTA_IMAGE_STATUS_INVALID_SIZE,
    SOTA_IMAGE_STATUS_CRC_ERROR
} SotaImage_Status_t;

typedef struct
{
    uint32 magic;
    uint8  protocolVersion;
    uint8  ecuType;
    uint16 reserved0;
    uint32 imageVersion;
    uint32 imageSize;
    uint32 imageCrc;
    uint32 manifestCrc;
    uint32 flags;
} SotaImage_Manifest_t;

typedef struct
{
    uint32 magic;
    uint16 headerVersion;
    uint16 headerSize;
    uint8  ecuType;
    uint8  reserved0[3];
    uint32 imageVersion;
    uint32 imageSize;
    uint32 imageCrc;
    uint32 vectorAddress;
    uint32 flags;
    uint32 headerCrc;
} SotaImage_Header_t;

SotaImage_Status_t SotaImage_ValidateManifest(const SotaImage_Manifest_t *manifest);
SotaImage_Status_t SotaImage_ValidateHeader(const SotaImage_Header_t *header);
boolean SotaImage_IsTargetEcuMatch(uint8 ecuType);

#endif /* SOTA_IMAGE_H_ */
