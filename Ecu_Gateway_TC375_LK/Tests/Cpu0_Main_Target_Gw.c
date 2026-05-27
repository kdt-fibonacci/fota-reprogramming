// /*********************************************************************************************************************/
// /*-----------------------------------------------------Includes------------------------------------------------------*/
// /*********************************************************************************************************************/

// #include <stdint.h>
// #include <string.h>
// #include <stdbool.h>

// #include "Ifx_Types.h"
// #include "IfxCpu.h"
// #include "IfxScuWdt.h"

// #include "Std_Types.h"
// #include "ComStack_Types.h"

// #include "UART.h"

// #include "Can.h"
// #include "Can_Cfg.h"
// #include "CanIf.h"
// #include "CanTp.h"
// #include "PduR.h"
// #include "DoIP.h"

// #include "SoAd_Cfg.h"
// #include "DoIP_Cfg.h"

// #include "Shared_Util_Time.h"

// /*********************************************************************************************************************/
// /*------------------------------------------------------Macros-------------------------------------------------------*/
// /*********************************************************************************************************************/

// #define TEST_MAIN_PERIOD_MS                            (1U)
// #define TEST_REQUEST_TIMEOUT_MS                        (10000U)

// #define TEST_DOIP_HEADER_LENGTH                        (8U)

// /*
//  * TransferData multi-block test
//  *
//  * One TransferData UDS request:
//  *   SID(1) + BlockSequenceCounter(1) + Data(512) = 514 bytes
//  *
//  * One DoIP Diagnostic Message:
//  *   DoIP Header(8) + Source/Target Address(4) + UDS(514) = 526 bytes
//  *
//  * Total transfer size:
//  *   512 bytes * 20 blocks = 10240 bytes = 0x00002800
//  */
// #define TEST_DOIP_MAX_PACKET_SIZE                      (1024U)

// #define TEST_TRANSFER_DATA_SIZE                        (510U)
// #define TEST_TRANSFER_BLOCK_COUNT                      (20U)
// #define TEST_TOTAL_TRANSFER_SIZE                       (TEST_TRANSFER_DATA_SIZE * TEST_TRANSFER_BLOCK_COUNT)

// #define TEST_DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQ  (0x0005U)
// #define TEST_DOIP_PAYLOAD_TYPE_DIAG_MESSAGE            (0x8001U)

// #define TEST_TESTER_LOGICAL_ADDRESS                    (DOIP_LOGICAL_ADDRESS_TESTER)
// #define TEST_TARGET_LOGICAL_ADDRESS                    (DOIP_LOGICAL_ADDRESS_TARGET_ECU)

// /*
//  * Logical test steps
//  *
//  * 0  : 10 03                 Extended Session
//  * 1  : 34 ...                RequestDownload
//  * 2  : 36 01 ~ 36 14         TransferData 20 blocks
//  * 3  : 37                    RequestTransferExit
//  * 4  : 31 01 FF 01           Verify
//  * 5  : 10 02                 Programming Session
//  * 6  : 31 01 FF 02           Bank Swap / Activation 예약
//  * 7  : 11 01                 ECUReset
//  * 8  : 22 F1 80              FOTA status read
//  * 9  : 31 01 FF 03           Rollback 예약
//  */
// #define TEST_LOGICAL_STEP_EXTENDED_SESSION             (0U)
// #define TEST_LOGICAL_STEP_REQUEST_DOWNLOAD             (1U)
// #define TEST_LOGICAL_STEP_TRANSFER_DATA                (2U)
// #define TEST_LOGICAL_STEP_TRANSFER_EXIT                (3U)
// #define TEST_LOGICAL_STEP_VERIFY                       (4U)
// #define TEST_LOGICAL_STEP_PROGRAMMING_SESSION          (5U)
// #define TEST_LOGICAL_STEP_BANK_SWAP                    (6U)
// #define TEST_LOGICAL_STEP_ECU_RESET                    (7U)
// #define TEST_LOGICAL_STEP_READ_FOTA_STATUS             (8U)
// #define TEST_LOGICAL_STEP_ROLLBACK                     (9U)

// #define TEST_LOGICAL_STEP_DONE                         (10U)

// /*********************************************************************************************************************/
// /*------------------------------------------------------Types--------------------------------------------------------*/
// /*********************************************************************************************************************/

// typedef enum
// {
//     TEST_MASTER_STATE_SEND_ROUTING_ACTIVATION = 0U,
//     TEST_MASTER_STATE_WAIT_ROUTING_ACTIVATION,

//     TEST_MASTER_STATE_SEND_UDS_REQUEST,
//     TEST_MASTER_STATE_WAIT_UDS_RESPONSE,

//     TEST_MASTER_STATE_DONE,
//     TEST_MASTER_STATE_ERROR
// } Test_MasterStateType;

// /*********************************************************************************************************************/
// /*------------------------------------------------Global Variables---------------------------------------------------*/
// /*********************************************************************************************************************/

// IfxCpu_syncEvent g_cpuSyncEvent;

// /*
//  * Gateway에서 Target 응답이 다시 DoIP_TpTransmit()까지 올라왔는지 확인하기 위한 테스트 플래그.
//  *
//  * 주의:
//  * 이 변수는 테스트용이다.
//  * DoIP_TpTransmit() 내부에서 Diagnostic Message Response를 생성한 뒤 TRUE로 세팅해줘야 한다.
//  */
// volatile boolean Test_DoIPResponseReceived = FALSE;

// /*********************************************************************************************************************/
// /*------------------------------------------------Static Variables---------------------------------------------------*/
// /*********************************************************************************************************************/

// static uint8 Test_DoIPPacket[TEST_DOIP_MAX_PACKET_SIZE];

// static volatile uint32 Test_Tick1ms = 0U;

// static Test_MasterStateType Test_MasterState = TEST_MASTER_STATE_SEND_ROUTING_ACTIVATION;
// static Test_MasterStateType Test_LastMasterState = TEST_MASTER_STATE_ERROR;

// static uint8  Test_LogicalStep = TEST_LOGICAL_STEP_EXTENDED_SESSION;
// static uint8  Test_TransferBlockIndex = 0U;
// static uint32 Test_RequestStartTick = 0U;

// /*********************************************************************************************************************/
// /*------------------------------------------------Private Functions--------------------------------------------------*/
// /*********************************************************************************************************************/

// static void Test_InitModules(void);
// static void Test_MainFunctions(void);
// static void Test_MockFotaMaster_MainFunction(void);

// static void Test_SendRoutingActivation(void);
// static void Test_SendCurrentUdsRequest(void);

// static void Test_SendUdsRequest(
//     const uint8* UdsPayloadPtr,
//     PduLengthType UdsPayloadLength
// );

// static void Test_SendTransferDataBlock(
//     uint8 BlockSequenceCounter
// );

// static void Test_SendDoIPPacketToGateway(
//     const uint8* PacketPtr,
//     PduLengthType PacketLength
// );

// static PduLengthType Test_BuildRoutingActivationRequest(
//     uint8* PacketPtr
// );

// static PduLengthType Test_BuildDiagnosticMessage(
//     uint8* PacketPtr,
//     const uint8* UdsPayloadPtr,
//     PduLengthType UdsPayloadLength
// );

// static void Test_OnUdsResponseReceived(void);

// static void Test_WriteUint16BigEndian(
//     uint8* DataPtr,
//     uint16 Value
// );

// static void Test_WriteUint32BigEndian(
//     uint8* DataPtr,
//     uint32 Value
// );

// static void Test_PrintHex(
//     const char* PrefixPtr,
//     const uint8* DataPtr,
//     PduLengthType Length
// );

// static const char* Test_GetMasterStateName(
//     Test_MasterStateType State
// );

// static const char* Test_GetLogicalStepName(
//     uint8 Step
// );

// static void Test_FillTransferDataBlock(
//     uint8* BufferPtr,
//     PduLengthType Length,
//     uint8 BlockSequenceCounter
// );

// /*********************************************************************************************************************/
// /*------------------------------------------------Main Function------------------------------------------------------*/
// /*********************************************************************************************************************/

// void core0_main(void)
// {
//     IfxCpu_enableInterrupts();

//     IfxScuWdt_disableCpuWatchdog(
//         IfxScuWdt_getCpuWatchdogPassword()
//     );

//     IfxScuWdt_disableSafetyWatchdog(
//         IfxScuWdt_getSafetyWatchdogPassword()
//     );

//     Test_InitModules();

//     UART_Printf("\r\n");
//     UART_Printf("========================================\r\n");
//     UART_Printf("[GW] Gateway FOTA UDS Sequence Test Start\r\n");
//     UART_Printf("========================================\r\n");
//     UART_Printf("[MOCK] TransferData block size=%u bytes, block count=%u, total=%lu bytes\r\n",
//                 TEST_TRANSFER_DATA_SIZE,
//                 TEST_TRANSFER_BLOCK_COUNT,
//                 (uint32)TEST_TOTAL_TRANSFER_SIZE);

//     while (1)
//     {
//         if (Test_LastMasterState != Test_MasterState)
//         {
//             UART_Printf(
//                 "[MOCK][STATE] %s -> %s Tick=%lu LogicalStep=%u(%s) TransferBlockIndex=%u ResponseFlag=%u\r\n",
//                 Test_GetMasterStateName(Test_LastMasterState),
//                 Test_GetMasterStateName(Test_MasterState),
//                 Test_Tick1ms,
//                 Test_LogicalStep,
//                 Test_GetLogicalStepName(Test_LogicalStep),
//                 Test_TransferBlockIndex,
//                 Test_DoIPResponseReceived
//             );

//             Test_LastMasterState = Test_MasterState;
//         }

//         Test_MainFunctions();

//         Test_MockFotaMaster_MainFunction();

//         Shared_Util_Time_DelayMs(TEST_MAIN_PERIOD_MS);

//         Test_Tick1ms++;
//     }
// }

// /*********************************************************************************************************************/
// /*------------------------------------------------Module Init/Main---------------------------------------------------*/
// /*********************************************************************************************************************/

// static void Test_InitModules(void)
// {
//     UART_Init();

//     UART_Printf("[INIT] UART_Init done\r\n");

//     Can_Init();
//     UART_Printf("[INIT] Can_Init done\r\n");

//     (void)Can_SetControllerMode(
//         CAN_CONTROLLER_0,
//         CAN_CS_STARTED
//     );
//     UART_Printf("[INIT] Can_SetControllerMode STARTED done\r\n");

//     CanIf_Init();
//     UART_Printf("[INIT] CanIf_Init done\r\n");

//     CanTp_Init();
//     UART_Printf("[INIT] CanTp_Init done\r\n");

//     PduR_Init();
//     UART_Printf("[INIT] PduR_Init done\r\n");

//     DoIP_Init();
//     UART_Printf("[INIT] DoIP_Init done\r\n");

//     /*
//      * 실제 TCP/LwIP/SoAd까지 같이 돌릴 때만 사용.
//      * 현재 Mock FOTA Master는 DoIP_TpRxIndication()을 직접 호출하므로 필수는 아니다.
//      *
//      * LwIP_Init();
//      * SoAd_Init();
//      */
// }

// static void Test_MainFunctions(void)
// {
//     Can_MainFunction_Read();

//     CanTp_MainFunction();

//     Can_MainFunction_Write();

//     /*
//      * 실제 TCP/LwIP/SoAd까지 같이 돌릴 때만 사용.
//      *
//      * LwIP_MainFunction();
//      * SoAd_MainFunction();
//      */
// }

// /*********************************************************************************************************************/
// /*------------------------------------------------Mock FOTA Master---------------------------------------------------*/
// /*********************************************************************************************************************/

// static void Test_MockFotaMaster_MainFunction(void)
// {
//     switch (Test_MasterState)
//     {
//         case TEST_MASTER_STATE_SEND_ROUTING_ACTIVATION:
//         {
//             UART_Printf("[MOCK] Send DoIP Routing Activation Request\r\n");

//             Test_SendRoutingActivation();

//             Test_RequestStartTick = Test_Tick1ms;
//             Test_MasterState = TEST_MASTER_STATE_WAIT_ROUTING_ACTIVATION;

//             UART_Printf(
//                 "[MOCK][RA] Request injected. Wait start tick=%lu\r\n",
//                 Test_RequestStartTick
//             );

//             break;
//         }

//         case TEST_MASTER_STATE_WAIT_ROUTING_ACTIVATION:
//         {
//             if ((Test_Tick1ms - Test_RequestStartTick) >= 100U)
//             {
//                 UART_Printf(
//                     "[MOCK][RA] Wait done. Elapsed=%lu ms\r\n",
//                     (Test_Tick1ms - Test_RequestStartTick)
//                 );

//                 Test_MasterState = TEST_MASTER_STATE_SEND_UDS_REQUEST;
//             }

//             break;
//         }

//         case TEST_MASTER_STATE_SEND_UDS_REQUEST:
//         {
//             UART_Printf(
//                 "[MOCK][UDS] Prepare request. LogicalStep=%u(%s) TransferBlockIndex=%u Tick=%lu\r\n",
//                 Test_LogicalStep,
//                 Test_GetLogicalStepName(Test_LogicalStep),
//                 Test_TransferBlockIndex,
//                 Test_Tick1ms
//             );

//             Test_DoIPResponseReceived = FALSE;

//             Test_SendCurrentUdsRequest();

//             Test_RequestStartTick = Test_Tick1ms;
//             Test_MasterState = TEST_MASTER_STATE_WAIT_UDS_RESPONSE;

//             UART_Printf(
//                 "[MOCK][UDS] Request injected. Wait start tick=%lu LogicalStep=%u(%s)\r\n",
//                 Test_RequestStartTick,
//                 Test_LogicalStep,
//                 Test_GetLogicalStepName(Test_LogicalStep)
//             );

//             break;
//         }

//         case TEST_MASTER_STATE_WAIT_UDS_RESPONSE:
//         {
//             if (Test_DoIPResponseReceived == TRUE)
//             {
//                 UART_Printf(
//                     "[MOCK] UDS response received. LogicalStep=%u(%s) TransferBlockIndex=%u Tick=%lu Elapsed=%lu ms\r\n",
//                     Test_LogicalStep,
//                     Test_GetLogicalStepName(Test_LogicalStep),
//                     Test_TransferBlockIndex,
//                     Test_Tick1ms,
//                     (Test_Tick1ms - Test_RequestStartTick)
//                 );

//                 Test_DoIPResponseReceived = FALSE;

//                 Test_OnUdsResponseReceived();

//                 if (Test_LogicalStep >= TEST_LOGICAL_STEP_DONE)
//                 {
//                     Test_MasterState = TEST_MASTER_STATE_DONE;
//                 }
//                 else
//                 {
//                     Test_MasterState = TEST_MASTER_STATE_SEND_UDS_REQUEST;
//                 }

//                 break;
//             }

//             if ((Test_Tick1ms - Test_RequestStartTick) > TEST_REQUEST_TIMEOUT_MS)
//             {
//                 UART_Printf(
//                     "[MOCK][ERROR] UDS response timeout. LogicalStep=%u(%s) TransferBlockIndex=%u Tick=%lu StartTick=%lu Elapsed=%lu ms ResponseFlag=%u\r\n",
//                     Test_LogicalStep,
//                     Test_GetLogicalStepName(Test_LogicalStep),
//                     Test_TransferBlockIndex,
//                     Test_Tick1ms,
//                     Test_RequestStartTick,
//                     (Test_Tick1ms - Test_RequestStartTick),
//                     Test_DoIPResponseReceived
//                 );

//                 UART_Printf(
//                     "[MOCK][ERROR] If DoIP TX log already appeared, check Test_DoIPResponseReceived flag in DoIP_TpTransmit().\r\n"
//                 );

//                 Test_MasterState = TEST_MASTER_STATE_ERROR;
//                 break;
//             }

//             break;
//         }

//         case TEST_MASTER_STATE_DONE:
//         {
//             static boolean DonePrinted = FALSE;

//             if (DonePrinted == FALSE)
//             {
//                 UART_Printf("[MOCK] All requested FOTA UDS sequence steps done. Final Tick=%lu\r\n", Test_Tick1ms);
//                 DonePrinted = TRUE;
//             }

//             break;
//         }

//         case TEST_MASTER_STATE_ERROR:
//         default:
//         {
//             break;
//         }
//     }
// }

// static void Test_SendCurrentUdsRequest(void)
// {
//     switch (Test_LogicalStep)
//     {
//         case TEST_LOGICAL_STEP_EXTENDED_SESSION:
//         {
//             /*
//              * Step 1
//              * Default -> Extended
//              *
//              * Request:
//              *   10 03
//              *
//              * Expected:
//              *   50 03 00 32 01 F4
//              */
//             const uint8 UdsRequest[] = { 0x10U, 0x03U };

//             UART_Printf("[MOCK][STEP1] Send UDS: 10 03 - Extended Session\r\n");

//             Test_PrintHex(
//                 "[MOCK][UDS-TX]",
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             break;
//         }

//         case TEST_LOGICAL_STEP_REQUEST_DOWNLOAD:
//         {
//             /*
//              * Step 2
//              * Extended
//              *
//              * RequestDownload
//              *
//              * Request format:
//              *   34 [DFI] [ALFI] [addr] [size]
//              *
//              * This test:
//              *   DFI  = 00
//              *   ALFI = 44
//              *   addr = 00 00 00 00
//              *   size = 00 00 28 00 = 10240 bytes
//              */
//             const uint8 UdsRequest[] =
//             {
//                 0x34U, 0x00U, 0x44U,
//                 0x00U, 0x00U, 0x00U, 0x00U,
//                 0x00U, 0x00U, 0x28U, 0x00U
//             };

//             UART_Printf("[MOCK][STEP2] Send UDS: 34 - RequestDownload, size=0x00002800\r\n");

//             Test_PrintHex(
//                 "[MOCK][UDS-TX]",
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             break;
//         }

//         case TEST_LOGICAL_STEP_TRANSFER_DATA:
//         {
//             /*
//              * Step 3
//              * Extended
//              *
//              * TransferData multi-block:
//              *   36 [BSC] [512 bytes data]
//              *
//              * BSC:
//              *   0x01 ~ 0x14
//              */
//             uint8 BlockSequenceCounter;

//             BlockSequenceCounter = (uint8)(Test_TransferBlockIndex + 1U);

//             Test_SendTransferDataBlock(BlockSequenceCounter);

//             break;
//         }

//         case TEST_LOGICAL_STEP_TRANSFER_EXIT:
//         {
//             /*
//              * Step 4
//              * Extended
//              *
//              * RequestTransferExit
//              *
//              * Request:
//              *   37
//              *
//              * Expected:
//              *   77
//              */
//             const uint8 UdsRequest[] = { 0x37U };

//             UART_Printf("[MOCK][STEP4] Send UDS: 37 - RequestTransferExit\r\n");

//             Test_PrintHex(
//                 "[MOCK][UDS-TX]",
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             break;
//         }

//         case TEST_LOGICAL_STEP_VERIFY:
//         {
//             /*
//              * Step 5
//              * Extended
//              *
//              * RoutineControl Verify
//              *
//              * Request:
//              *   31 01 FF 01
//              *
//              * Expected:
//              *   71 01 FF 01
//              */
//             const uint8 UdsRequest[] =
//             {
//                 0x31U, 0x01U, 0xFFU, 0x00U
//             };

//             UART_Printf("[MOCK][STEP5] Send UDS: 31 01 FF 01 - Verify\r\n");

//             Test_PrintHex(
//                 "[MOCK][UDS-TX]",
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             break;
//         }

//         case TEST_LOGICAL_STEP_PROGRAMMING_SESSION:
//         {
//             /*
//              * Step 6
//              * Extended -> Programming
//              *
//              * Request:
//              *   10 02
//              *
//              * Expected:
//              *   50 02 00 32 01 F4
//              */
//             const uint8 UdsRequest[] = { 0x10U, 0x02U };

//             UART_Printf("[MOCK][STEP6] Send UDS: 10 02 - Programming Session\r\n");

//             Test_PrintHex(
//                 "[MOCK][UDS-TX]",
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             break;
//         }

//         case TEST_LOGICAL_STEP_BANK_SWAP:
//         {
//             /*
//              * Step 7
//              * Programming
//              *
//              * RoutineControl Bank Swap / Activation
//              *
//              * Request:
//              *   31 01 FF 02
//              *
//              * Expected:
//              *   71 01 FF 02
//              */
//             const uint8 UdsRequest[] =
//             {
//                 0x31U, 0x01U, 0xFFU, 0x01U
//             };

//             UART_Printf("[MOCK][STEP7] Send UDS: 31 01 FF 02 - Bank Swap\r\n");

//             Test_PrintHex(
//                 "[MOCK][UDS-TX]",
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             break;
//         }

//         case TEST_LOGICAL_STEP_ECU_RESET:
//         {
//             /*
//              * Step 8
//              * Programming
//              *
//              * ECUReset
//              *
//              * Request:
//              *   11 01
//              *
//              * Expected:
//              *   51 01
//              */
//             const uint8 UdsRequest[] = { 0x11U, 0x01U };

//             UART_Printf("[MOCK][STEP8] Send UDS: 11 01 - ECU Reset\r\n");

//             Test_PrintHex(
//                 "[MOCK][UDS-TX]",
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             break;
//         }

//         case TEST_LOGICAL_STEP_READ_FOTA_STATUS:
//         {
//             /*
//              * Step 9
//              * Any Session
//              *
//              * ReadDataByIdentifier - FOTA status
//              *
//              * Request:
//              *   22 F1 80
//              *
//              * Expected example:
//              *   62 F1 80 06
//              */
//             const uint8 UdsRequest[] =
//             {
//                 0x22U, 0xF1U, 0x80U
//             };

//             UART_Printf("[MOCK][STEP9] Send UDS: 22 F1 80 - Read FOTA Status\r\n");

//             Test_PrintHex(
//                 "[MOCK][UDS-TX]",
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             break;
//         }

//         case TEST_LOGICAL_STEP_ROLLBACK:
//         {
//             /*
//              * Step 10
//              * Programming
//              *
//              * RoutineControl Rollback
//              *
//              * Request:
//              *   31 01 FF 03
//              *
//              * Expected:
//              *   71 01 FF 03
//              */
//             const uint8 UdsRequest[] =
//             {
//                 0x31U, 0x01U, 0xFFU, 0x02U
//             };

//             UART_Printf("[MOCK][STEP10] Send UDS: 31 01 FF 03 - Rollback\r\n");

//             Test_PrintHex(
//                 "[MOCK][UDS-TX]",
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             break;
//         }

//         default:
//         {
//             UART_Printf("[MOCK][ERROR] Unknown logical step=%u\r\n", Test_LogicalStep);
//             break;
//         }
//     }
// }

// static void Test_OnUdsResponseReceived(void)
// {
//     if (Test_LogicalStep == TEST_LOGICAL_STEP_TRANSFER_DATA)
//     {
//         Test_TransferBlockIndex++;

//         UART_Printf(
//             "[MOCK][TRANSFER] Completed block %u/%u\r\n",
//             Test_TransferBlockIndex,
//             TEST_TRANSFER_BLOCK_COUNT
//         );

//         if (Test_TransferBlockIndex >= TEST_TRANSFER_BLOCK_COUNT)
//         {
//             Test_LogicalStep = TEST_LOGICAL_STEP_TRANSFER_EXIT;
//         }

//         return;
//     }

//     Test_LogicalStep++;
// }

// static void Test_SendTransferDataBlock(
//     uint8 BlockSequenceCounter
// )
// {
//     /*
//      * TransferData block N - Multi Frame test
//      *
//      * UDS Request:
//      *   36 [BSC] [512 bytes data]
//      *
//      * UDS Length:
//      *   SID(1) + BlockSequenceCounter(1) + Data(512) = 514 bytes
//      *
//      * Expected CanTp:
//      *   First Frame + Consecutive Frames
//      *
//      * Expected UDS Response:
//      *   76 [BSC]
//      */
//     static uint8 UdsRequest[2U + TEST_TRANSFER_DATA_SIZE];

//     UdsRequest[0] = 0x36U;
//     UdsRequest[1] = BlockSequenceCounter;

//     Test_FillTransferDataBlock(
//         &UdsRequest[2],
//         TEST_TRANSFER_DATA_SIZE,
//         BlockSequenceCounter
//     );

//     UART_Printf(
//         "[MOCK][STEP3] Send UDS: 36 %02X - TransferData block %u/%u, 512 bytes\r\n",
//         BlockSequenceCounter,
//         BlockSequenceCounter,
//         TEST_TRANSFER_BLOCK_COUNT
//     );

//     UART_Printf(
//         "[MOCK][TRANSFER] Block=%u/%u UdsLen=%u DataLen=%u Expected=CanTp MultiFrame\r\n",
//         BlockSequenceCounter,
//         TEST_TRANSFER_BLOCK_COUNT,
//         sizeof(UdsRequest),
//         TEST_TRANSFER_DATA_SIZE
//     );

//     Test_PrintHex(
//         "[MOCK][UDS-TX]",
//         UdsRequest,
//         sizeof(UdsRequest)
//     );

//     Test_SendUdsRequest(
//         UdsRequest,
//         sizeof(UdsRequest)
//     );
// }

// static void Test_SendRoutingActivation(void)
// {
//     PduLengthType PacketLength;

//     PacketLength = Test_BuildRoutingActivationRequest(
//         Test_DoIPPacket
//     );

//     Test_PrintHex(
//         "[MOCK][RA-PACKET]",
//         Test_DoIPPacket,
//         PacketLength
//     );

//     Test_SendDoIPPacketToGateway(
//         Test_DoIPPacket,
//         PacketLength
//     );
// }

// static void Test_SendUdsRequest(
//     const uint8* UdsPayloadPtr,
//     PduLengthType UdsPayloadLength
// )
// {
//     PduLengthType PacketLength;

//     PacketLength = Test_BuildDiagnosticMessage(
//         Test_DoIPPacket,
//         UdsPayloadPtr,
//         UdsPayloadLength
//     );

//     Test_PrintHex(
//         "[MOCK][DOIP-DIAG-PACKET]",
//         Test_DoIPPacket,
//         PacketLength
//     );

//     Test_SendDoIPPacketToGateway(
//         Test_DoIPPacket,
//         PacketLength
//     );
// }

// static void Test_SendDoIPPacketToGateway(
//     const uint8* PacketPtr,
//     PduLengthType PacketLength
// )
// {
//     PduInfoType PduInfo;

//     if (PacketPtr == NULL_PTR)
//     {
//         UART_Printf("[MOCK][ERROR] PacketPtr is NULL\r\n");
//         return;
//     }

//     if (PacketLength == 0U)
//     {
//         UART_Printf("[MOCK][ERROR] PacketLength is zero\r\n");
//         return;
//     }

//     if (PacketLength > TEST_DOIP_MAX_PACKET_SIZE)
//     {
//         UART_Printf(
//             "[MOCK][ERROR] PacketLength=%u exceeds TEST_DOIP_MAX_PACKET_SIZE=%u\r\n",
//             PacketLength,
//             TEST_DOIP_MAX_PACKET_SIZE
//         );

//         return;
//     }

//     PduInfo.SduDataPtr = (uint8*)PacketPtr;
//     PduInfo.SduLength  = PacketLength;

//     UART_Printf(
//         "[MOCK] Inject DoIP packet to Gateway. SoAdRxPduId=%u Len=%u Tick=%lu\r\n",
//         SOAD_RXPDU_DOIP_TCP,
//         PacketLength,
//         Test_Tick1ms
//     );

//     Test_PrintHex(
//         "[MOCK][INJECT]",
//         PacketPtr,
//         PacketLength
//     );

//     /*
//      * 실제 TCP 수신 대신 SoAd가 DoIP로 올려준 상황을 직접 만든다.
//      */
//     DoIP_TpRxIndication(
//         SOAD_RXPDU_DOIP_TCP,
//         &PduInfo
//     );

//     UART_Printf("[MOCK] DoIP_TpRxIndication returned\r\n");
// }

// /*********************************************************************************************************************/
// /*------------------------------------------------DoIP Packet Builder-----------------------------------------------*/
// /*********************************************************************************************************************/

// static PduLengthType Test_BuildRoutingActivationRequest(
//     uint8* PacketPtr
// )
// {
//     uint8* PayloadPtr;
//     uint32 PayloadLength;

//     PayloadLength = 3U;

//     PacketPtr[0] = DOIP_PROTOCOL_VERSION;
//     PacketPtr[1] = DOIP_INVERSE_PROTOCOL_VERSION;

//     Test_WriteUint16BigEndian(
//         &PacketPtr[2],
//         TEST_DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQ
//     );

//     Test_WriteUint32BigEndian(
//         &PacketPtr[4],
//         PayloadLength
//     );

//     PayloadPtr = &PacketPtr[TEST_DOIP_HEADER_LENGTH];

//     Test_WriteUint16BigEndian(
//         &PayloadPtr[0],
//         TEST_TESTER_LOGICAL_ADDRESS
//     );

//     PayloadPtr[2] = 0x00U;

//     UART_Printf(
//         "[MOCK][BUILD-RA] PayloadLength=%lu TesterSA=0x%04X ActivationType=0x%02X TotalLength=%u\r\n",
//         PayloadLength,
//         TEST_TESTER_LOGICAL_ADDRESS,
//         PayloadPtr[2],
//         (PduLengthType)(TEST_DOIP_HEADER_LENGTH + PayloadLength)
//     );

//     return (PduLengthType)(TEST_DOIP_HEADER_LENGTH + PayloadLength);
// }

// static PduLengthType Test_BuildDiagnosticMessage(
//     uint8* PacketPtr,
//     const uint8* UdsPayloadPtr,
//     PduLengthType UdsPayloadLength
// )
// {
//     uint8* PayloadPtr;
//     uint32 PayloadLength;

//     if ((PacketPtr == NULL_PTR) || (UdsPayloadPtr == NULL_PTR))
//     {
//         return 0U;
//     }

//     PayloadLength = (uint32)UdsPayloadLength + 4U;

//     if ((TEST_DOIP_HEADER_LENGTH + PayloadLength) > TEST_DOIP_MAX_PACKET_SIZE)
//     {
//         UART_Printf(
//             "[MOCK][ERROR] DoIP packet too large. Total=%lu Max=%u UdsLen=%u\r\n",
//             (uint32)(TEST_DOIP_HEADER_LENGTH + PayloadLength),
//             TEST_DOIP_MAX_PACKET_SIZE,
//             UdsPayloadLength
//         );

//         return 0U;
//     }

//     PacketPtr[0] = DOIP_PROTOCOL_VERSION;
//     PacketPtr[1] = DOIP_INVERSE_PROTOCOL_VERSION;

//     Test_WriteUint16BigEndian(
//         &PacketPtr[2],
//         TEST_DOIP_PAYLOAD_TYPE_DIAG_MESSAGE
//     );

//     Test_WriteUint32BigEndian(
//         &PacketPtr[4],
//         PayloadLength
//     );

//     PayloadPtr = &PacketPtr[TEST_DOIP_HEADER_LENGTH];

//     /*
//      * DoIP Diagnostic Message Payload:
//      *
//      * Byte 0~1 : SourceAddress = Tester
//      * Byte 2~3 : TargetAddress = Target ECU
//      * Byte 4~  : UDS Payload
//      */
//     Test_WriteUint16BigEndian(
//         &PayloadPtr[0],
//         TEST_TESTER_LOGICAL_ADDRESS
//     );

//     Test_WriteUint16BigEndian(
//         &PayloadPtr[2],
//         TEST_TARGET_LOGICAL_ADDRESS
//     );

//     memcpy(
//         &PayloadPtr[4],
//         UdsPayloadPtr,
//         UdsPayloadLength
//     );

//     UART_Printf(
//         "[MOCK][BUILD-DIAG] PayloadLength=%lu Source=0x%04X Target=0x%04X UdsLen=%u TotalLength=%u\r\n",
//         PayloadLength,
//         TEST_TESTER_LOGICAL_ADDRESS,
//         TEST_TARGET_LOGICAL_ADDRESS,
//         UdsPayloadLength,
//         (PduLengthType)(TEST_DOIP_HEADER_LENGTH + PayloadLength)
//     );

//     return (PduLengthType)(TEST_DOIP_HEADER_LENGTH + PayloadLength);
// }

// /*********************************************************************************************************************/
// /*------------------------------------------------Utility Functions--------------------------------------------------*/
// /*********************************************************************************************************************/

// static void Test_WriteUint16BigEndian(
//     uint8* DataPtr,
//     uint16 Value
// )
// {
//     DataPtr[0] = (uint8)((Value >> 8U) & 0xFFU);
//     DataPtr[1] = (uint8)(Value & 0xFFU);
// }

// static void Test_WriteUint32BigEndian(
//     uint8* DataPtr,
//     uint32 Value
// )
// {
//     DataPtr[0] = (uint8)((Value >> 24U) & 0xFFU);
//     DataPtr[1] = (uint8)((Value >> 16U) & 0xFFU);
//     DataPtr[2] = (uint8)((Value >> 8U) & 0xFFU);
//     DataPtr[3] = (uint8)(Value & 0xFFU);
// }

// static void Test_PrintHex(
//     const char* PrefixPtr,
//     const uint8* DataPtr,
//     PduLengthType Length
// )
// {
//     PduLengthType Index;
//     PduLengthType PrintLength;

//     if (PrefixPtr == NULL_PTR)
//     {
//         PrefixPtr = "[HEX]";
//     }

//     if (DataPtr == NULL_PTR)
//     {
//         UART_Printf("%s NULL\r\n", PrefixPtr);
//         return;
//     }

//     /*
//      * 대용량 TransferData는 UART 로그가 너무 길어지므로 앞쪽 64바이트만 출력한다.
//      */
//     PrintLength = Length;

//     if (PrintLength > 64U)
//     {
//         PrintLength = 64U;
//     }

//     UART_Printf("%s Len=%u Data=", PrefixPtr, Length);

//     for (Index = 0U; Index < PrintLength; Index++)
//     {
//         UART_Printf(" %02X", DataPtr[Index]);
//     }

//     if (PrintLength < Length)
//     {
//         UART_Printf(" ...");
//     }

//     UART_Printf("\r\n");
// }

// static const char* Test_GetMasterStateName(
//     Test_MasterStateType State
// )
// {
//     switch (State)
//     {
//         case TEST_MASTER_STATE_SEND_ROUTING_ACTIVATION:
//             return "SEND_ROUTING_ACTIVATION";

//         case TEST_MASTER_STATE_WAIT_ROUTING_ACTIVATION:
//             return "WAIT_ROUTING_ACTIVATION";

//         case TEST_MASTER_STATE_SEND_UDS_REQUEST:
//             return "SEND_UDS_REQUEST";

//         case TEST_MASTER_STATE_WAIT_UDS_RESPONSE:
//             return "WAIT_UDS_RESPONSE";

//         case TEST_MASTER_STATE_DONE:
//             return "DONE";

//         case TEST_MASTER_STATE_ERROR:
//             return "ERROR";

//         default:
//             return "UNKNOWN";
//     }
// }

// static const char* Test_GetLogicalStepName(
//     uint8 Step
// )
// {
//     switch (Step)
//     {
//         case TEST_LOGICAL_STEP_EXTENDED_SESSION:
//             return "ExtendedSession";

//         case TEST_LOGICAL_STEP_REQUEST_DOWNLOAD:
//             return "RequestDownload";

//         case TEST_LOGICAL_STEP_TRANSFER_DATA:
//             return "TransferData";

//         case TEST_LOGICAL_STEP_TRANSFER_EXIT:
//             return "TransferExit";

//         case TEST_LOGICAL_STEP_VERIFY:
//             return "Verify";

//         case TEST_LOGICAL_STEP_PROGRAMMING_SESSION:
//             return "ProgrammingSession";

//         case TEST_LOGICAL_STEP_BANK_SWAP:
//             return "BankSwap";

//         case TEST_LOGICAL_STEP_ECU_RESET:
//             return "ECUReset";

//         case TEST_LOGICAL_STEP_READ_FOTA_STATUS:
//             return "ReadFotaStatus";

//         case TEST_LOGICAL_STEP_ROLLBACK:
//             return "Rollback";

//         case TEST_LOGICAL_STEP_DONE:
//             return "Done";

//         default:
//             return "Unknown";
//     }
// }

// static void Test_FillTransferDataBlock(
//     uint8* BufferPtr,
//     PduLengthType Length,
//     uint8 BlockSequenceCounter
// )
// {
//     PduLengthType Index;

//     if (BufferPtr == NULL_PTR)
//     {
//         return;
//     }

//     for (Index = 0U; Index < Length; Index++)
//     {
//         /*
//          * Block마다 다른 반복 패턴.
//          *
//          * Block 1 : 10 11 12 ... 1F 10 ...
//          * Block 2 : 20 21 22 ... 2F 20 ...
//          * ...
//          * Block 15: F0 F1 F2 ... FF F0 ...
//          * Block 16: 00 01 02 ... 0F 00 ...
//          */
//         BufferPtr[Index] = (uint8)((BlockSequenceCounter << 4U) + (Index & 0x0FU));
//     }
// }