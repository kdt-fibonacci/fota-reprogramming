#ifndef SOTA_CRC32_H
#define SOTA_CRC32_H 1

#include <stdint.h>
#include <Ifx_Types.h>

#define CRC32_POLYNOMIAL 0xedb88320ull

#ifdef __cplusplus
extern "C" {
#endif /* SOTA_CRC32_H */

uint32 crc32(uint32 crc, const uint8 *buf, uint32 len);

#ifdef __cplusplus
}
#endif

#endif
