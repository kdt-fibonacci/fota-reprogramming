/*
 * Sota_SwapDiag.h
 *
 * Read-only TC37x swap diagnostics.
 */

#ifndef SOTA_SWAP_DIAG_H
#define SOTA_SWAP_DIAG_H 1

/******************************************************************************/
/*----------------------------------Includes----------------------------------*/
/******************************************************************************/
#include <Ifx_Types.h>
#include <Sota/Sota_Tc37x_Config.h>

/******************************************************************************/
/*-------------------------Function Prototypes--------------------------------*/
/******************************************************************************/
typedef struct
{
    uint32 stmem1;
    uint32 stmem2;
    uint32 swapctrl;
    uint32 procontp;

    uint8 swapEn;
    uint8 swapCfg;
    uint8 swapTarget;
    uint8 swapDwIndex;
    uint8 swapEntryIndex;

    uint32 bootAddr;
    uint8 currentMode;
} SotaSwap_MinDiag_t;

typedef struct
{
    uint32 baseAddr;
    uint32 word[8];
    uint8 readOk;
} SotaSwap_UcbHeadDump_t;

typedef enum
{
    SOTA_PROVISION_OK = 0u,
    SOTA_PROVISION_DISABLED = 1u,
    SOTA_PROVISION_ALREADY_ENABLED = 2u,
    SOTA_PROVISION_INVALID_UCB = 3u,
    SOTA_PROVISION_SWAP_ENTRY_INVALID = 4u,
    SOTA_PROVISION_OTP_INVALID = 5u,
    SOTA_PROVISION_WRITE_FAILED = 6u,
    SOTA_PROVISION_VERIFY_FAILED = 7u,
    SOTA_PROVISION_NO_FREE_SWAP_ENTRY = 8u
} SotaProvision_Result;

#define SOTA_PROV_STAGE_OTP_CHECK_ORIG          (0x1101u)
#define SOTA_PROV_STAGE_OTP_CHECK_COPY          (0x1102u)
#define SOTA_PROV_STAGE_OTP_PROCONTP_INVALID    (0x1201u)
#define SOTA_PROV_STAGE_OTP_PROCONTP_PAIR_USED  (0x1202u)
#define SOTA_PROV_STAGE_OTP_CONFIRM_INVALID     (0x1203u)
#define SOTA_PROV_STAGE_OTP_CONFIRM_PAIR_USED   (0x1204u)
#define SOTA_PROV_STAGE_OTP_ENABLE_ORIG         (0x1301u)
#define SOTA_PROV_STAGE_OTP_ENABLE_COPY         (0x1302u)
#define SOTA_PROV_STAGE_SWAP_ERASE_ORIG         (0x2101u)
#define SOTA_PROV_STAGE_SWAP_ERASE_COPY         (0x2102u)
#define SOTA_PROV_STAGE_SWAP_REWRITE_ORIG       (0x2201u)
#define SOTA_PROV_STAGE_SWAP_REWRITE_COPY       (0x2202u)
#define SOTA_PROV_STAGE_DONE                    (0x9000u)

extern volatile uint32 g_sotaProvisionStage;
extern volatile uint8 g_sotaProvisionLastUcbNo;
extern volatile uint32 g_sotaProvisionLastBaseAddr;
extern volatile uint32 g_sotaProvisionLastWords[4];

extern uint8 SotaSwap_GetCurrentMode(void);
extern uint8 SotaSwap_CheckSwapActive(void);
extern void SotaSwap_MinDiag_Update(volatile SotaSwap_MinDiag_t *diag);
extern void SotaSwap_ReadUcbHeads(volatile SotaSwap_UcbHeadDump_t *otpOrig,
                                  volatile SotaSwap_UcbHeadDump_t *otpCopy,
                                  volatile SotaSwap_UcbHeadDump_t *swapOrig,
                                  volatile SotaSwap_UcbHeadDump_t *swapCopy);
extern SotaProvision_Result SotaProvision_ProgramSwapEntry0Standard(void);
extern SotaProvision_Result SotaProvision_ReinitSwapEntry0Standard(uint32 *origEraseResult,
                                                                   uint32 *copyEraseResult);
extern SotaProvision_Result SotaProvision_ProgramNextSwapEntry(uint32 *entryIndex,
                                                               uint32 *targetModeWord);
extern SotaProvision_Result SotaProvision_EnableSotaInOtp0(void);
extern SotaProvision_Result SotaProvision_RunInitialOnce(void);

#endif /* SOTA_SWAP_DIAG_H */
