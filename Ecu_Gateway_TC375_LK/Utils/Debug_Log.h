#ifndef DEBUG_LOG_H_
#define DEBUG_LOG_H_

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "Debug_Cfg.h"
#include "Std_Types.h"
#include "ComStack_Types.h"

#if ((DEBUG_CAN_ENABLE == 1U) ||       \
     (DEBUG_CANIF_ENABLE == 1U) ||     \
     (DEBUG_CANTP_ENABLE == 1U) ||     \
     (DEBUG_PDUR_ENABLE == 1U) ||      \
     (DEBUG_DCM_ENABLE == 1U) ||       \
     (DEBUG_DOIP_ENABLE == 1U) ||      \
     (DEBUG_SOAD_ENABLE == 1U))
#include "UART.h"
#endif

/*********************************************************************************************************************/
/*-----------------------------------------------Common Debug Macros-------------------------------------------------*/
/*********************************************************************************************************************/

#define DEBUG_LOG_PRINT_DATA_IMPL(printfMacro, dataPtr, length)       \
    do                                                               \
    {                                                                \
        PduLengthType DebugIndex;                                    \
                                                                     \
        if ((dataPtr) == NULL_PTR)                                   \
        {                                                            \
            printfMacro(" <NULL>\r\n");                             \
        }                                                            \
        else                                                         \
        {                                                            \
            for (DebugIndex = 0U;                                    \
                 DebugIndex < (PduLengthType)(length);               \
                 DebugIndex++)                                       \
            {                                                        \
                printfMacro(                                         \
                    " %02X",                                        \
                    (dataPtr)[DebugIndex]                            \
                );                                                   \
            }                                                        \
                                                                     \
            printfMacro("\r\n");                                    \
        }                                                            \
    } while (0)

#define DEBUG_LOG_PRINT_PDU_IMPL(printfMacro, prefix, pduInfoPtr)     \
    do                                                               \
    {                                                                \
        if ((pduInfoPtr) == NULL_PTR)                                \
        {                                                            \
            printfMacro(                                             \
                "%s PduInfo=NULL\r\n",                              \
                (prefix)                                             \
            );                                                       \
        }                                                            \
        else                                                         \
        {                                                            \
            printfMacro(                                             \
                "%s Len=%u Data=",                                   \
                (prefix),                                            \
                (pduInfoPtr)->SduLength                              \
            );                                                       \
                                                                     \
            DEBUG_LOG_PRINT_DATA_IMPL(                               \
                printfMacro,                                         \
                (pduInfoPtr)->SduDataPtr,                            \
                (pduInfoPtr)->SduLength                              \
            );                                                       \
        }                                                            \
    } while (0)

/*********************************************************************************************************************/
/*------------------------------------------------CAN Debug Macros---------------------------------------------------*/
/*********************************************************************************************************************/

#if (DEBUG_CAN_ENABLE == 1U)

#define CAN_DEBUG_PRINTF(...)                                        \
    do                                                              \
    {                                                               \
        UART_Printf(__VA_ARGS__);                                   \
    } while (0)

#define CAN_DEBUG_PRINT_DATA(dataPtr, length)                        \
    do                                                              \
    {                                                               \
        DEBUG_LOG_PRINT_DATA_IMPL(                                  \
            CAN_DEBUG_PRINTF,                                       \
            (dataPtr),                                              \
            (length)                                                \
        );                                                          \
    } while (0)

#define CAN_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                      \
    do                                                              \
    {                                                               \
        DEBUG_LOG_PRINT_PDU_IMPL(                                   \
            CAN_DEBUG_PRINTF,                                       \
            (prefix),                                               \
            (pduInfoPtr)                                            \
        );                                                          \
    } while (0)

#else

#define CAN_DEBUG_PRINTF(...)                                        \
    do                                                              \
    {                                                               \
    } while (0)

#define CAN_DEBUG_PRINT_DATA(dataPtr, length)                        \
    do                                                              \
    {                                                               \
        (void)(dataPtr);                                            \
        (void)(length);                                             \
    } while (0)

#define CAN_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                      \
    do                                                              \
    {                                                               \
        (void)(prefix);                                             \
        (void)(pduInfoPtr);                                         \
    } while (0)

#endif

/*********************************************************************************************************************/
/*------------------------------------------------CanIf Debug Macros-------------------------------------------------*/
/*********************************************************************************************************************/

#if (DEBUG_CANIF_ENABLE == 1U)

#define CANIF_DEBUG_PRINTF(...)                                      \
    do                                                              \
    {                                                               \
        UART_Printf(__VA_ARGS__);                                   \
    } while (0)

#define CANIF_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                    \
    do                                                              \
    {                                                               \
        DEBUG_LOG_PRINT_PDU_IMPL(                                   \
            CANIF_DEBUG_PRINTF,                                     \
            (prefix),                                               \
            (pduInfoPtr)                                            \
        );                                                          \
    } while (0)

#else

#define CANIF_DEBUG_PRINTF(...)                                      \
    do                                                              \
    {                                                               \
    } while (0)

#define CANIF_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                    \
    do                                                              \
    {                                                               \
        (void)(prefix);                                             \
        (void)(pduInfoPtr);                                         \
    } while (0)

#endif

/*********************************************************************************************************************/
/*------------------------------------------------CanTp Debug Macros-------------------------------------------------*/
/*********************************************************************************************************************/

#if (DEBUG_CANTP_ENABLE == 1U)

#define CANTP_DEBUG_PRINTF(...)                                      \
    do                                                              \
    {                                                               \
        UART_Printf(__VA_ARGS__);                                   \
    } while (0)

#define CANTP_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                    \
    do                                                              \
    {                                                               \
        DEBUG_LOG_PRINT_PDU_IMPL(                                   \
            CANTP_DEBUG_PRINTF,                                     \
            (prefix),                                               \
            (pduInfoPtr)                                            \
        );                                                          \
    } while (0)

#else

#define CANTP_DEBUG_PRINTF(...)                                      \
    do                                                              \
    {                                                               \
    } while (0)

#define CANTP_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                    \
    do                                                              \
    {                                                               \
        (void)(prefix);                                             \
        (void)(pduInfoPtr);                                         \
    } while (0)

#endif

/*********************************************************************************************************************/
/*-------------------------------------------------PduR Debug Macros-------------------------------------------------*/
/*********************************************************************************************************************/

#if (DEBUG_PDUR_ENABLE == 1U)

#define PDUR_DEBUG_PRINTF(...)                                       \
    do                                                              \
    {                                                               \
        UART_Printf(__VA_ARGS__);                                   \
    } while (0)

#define PDUR_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                     \
    do                                                              \
    {                                                               \
        DEBUG_LOG_PRINT_PDU_IMPL(                                   \
            PDUR_DEBUG_PRINTF,                                      \
            (prefix),                                               \
            (pduInfoPtr)                                            \
        );                                                          \
    } while (0)

#else

#define PDUR_DEBUG_PRINTF(...)                                       \
    do                                                              \
    {                                                               \
    } while (0)

#define PDUR_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                     \
    do                                                              \
    {                                                               \
        (void)(prefix);                                             \
        (void)(pduInfoPtr);                                         \
    } while (0)

#endif

/*********************************************************************************************************************/
/*--------------------------------------------------Dcm Debug Macros-------------------------------------------------*/
/*********************************************************************************************************************/

#if (DEBUG_DCM_ENABLE == 1U)

#define DCM_DEBUG_PRINTF(...)                                        \
    do                                                              \
    {                                                               \
        UART_Printf(__VA_ARGS__);                                   \
    } while (0)

#define DCM_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                      \
    do                                                              \
    {                                                               \
        DEBUG_LOG_PRINT_PDU_IMPL(                                   \
            DCM_DEBUG_PRINTF,                                       \
            (prefix),                                               \
            (pduInfoPtr)                                            \
        );                                                          \
    } while (0)

#else

#define DCM_DEBUG_PRINTF(...)                                        \
    do                                                              \
    {                                                               \
    } while (0)

#define DCM_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                      \
    do                                                              \
    {                                                               \
        (void)(prefix);                                             \
        (void)(pduInfoPtr);                                         \
    } while (0)

#endif

/*********************************************************************************************************************/
/*-------------------------------------------------DoIP Debug Macros-------------------------------------------------*/
/*********************************************************************************************************************/

#if (DEBUG_DOIP_ENABLE == 1U)

#define DOIP_DEBUG_PRINTF(...)                                       \
    do                                                              \
    {                                                               \
        UART_Printf(__VA_ARGS__);                                   \
    } while (0)

#define DOIP_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                     \
    do                                                              \
    {                                                               \
        DEBUG_LOG_PRINT_PDU_IMPL(                                   \
            DOIP_DEBUG_PRINTF,                                      \
            (prefix),                                               \
            (pduInfoPtr)                                            \
        );                                                          \
    } while (0)

#else

#define DOIP_DEBUG_PRINTF(...)                                       \
    do                                                              \
    {                                                               \
    } while (0)

#define DOIP_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                     \
    do                                                              \
    {                                                               \
        (void)(prefix);                                             \
        (void)(pduInfoPtr);                                         \
    } while (0)

#endif

/*********************************************************************************************************************/
/*-------------------------------------------------SoAd Debug Macros-------------------------------------------------*/
/*********************************************************************************************************************/

#if (DEBUG_SOAD_ENABLE == 1U)

#define SOAD_DEBUG_PRINTF(...)                                       \
    do                                                              \
    {                                                               \
        UART_Printf(__VA_ARGS__);                                   \
    } while (0)

#define SOAD_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                     \
    do                                                              \
    {                                                               \
        DEBUG_LOG_PRINT_PDU_IMPL(                                   \
            SOAD_DEBUG_PRINTF,                                      \
            (prefix),                                               \
            (pduInfoPtr)                                            \
        );                                                          \
    } while (0)

#else

#define SOAD_DEBUG_PRINTF(...)                                       \
    do                                                              \
    {                                                               \
    } while (0)

#define SOAD_DEBUG_PRINT_PDU(prefix, pduInfoPtr)                     \
    do                                                              \
    {                                                               \
        (void)(prefix);                                             \
        (void)(pduInfoPtr);                                         \
    } while (0)

#endif

#endif /* DEBUG_LOG_H_ */
