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
// #include "CanIf.h"
// #include "CanTp.h"
// #include "PduR.h"
// #include "DoIP.h"
// #include "SoAd_Cfg.h"
// #include "DoIP_Cfg.h"

// /*********************************************************************************************************************/
// /*------------------------------------------------------Macros-------------------------------------------------------*/
// /*********************************************************************************************************************/

// #define TEST_MAIN_PERIOD_MS                  (1U)
// #define TEST_SEND_PERIOD_MS                  (1000U)

// #define TEST_DOIP_HEADER_LENGTH              (8U)
// #define TEST_DOIP_MAX_PACKET_SIZE            (256U)

// #define TEST_DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQ   (0x0005U)
// #define TEST_DOIP_PAYLOAD_TYPE_DIAG_MESSAGE             (0x8001U)

// #define TEST_TESTER_LOGICAL_ADDRESS          (DOIP_LOGICAL_ADDRESS_TESTER)
// #define TEST_TARGET_LOGICAL_ADDRESS          (DOIP_LOGICAL_ADDRESS_TARGET_ECU_0)

// /*********************************************************************************************************************/
// /*------------------------------------------------Static Variables---------------------------------------------------*/
// /*********************************************************************************************************************/

// static uint8 Test_DoIPPacket[TEST_DOIP_MAX_PACKET_SIZE];

// static uint32 Test_Tick1ms = 0U;
// static uint8 Test_Step = 0U;
// static boolean Test_RoutingActivationSent = FALSE;

// /*********************************************************************************************************************/
// /*------------------------------------------------Private Functions--------------------------------------------------*/
// /*********************************************************************************************************************/

// static void Test_WriteUint16BigEndian(
//     uint8* DataPtr,
//     uint16 Value
// );

// static void Test_WriteUint32BigEndian(
//     uint8* DataPtr,
//     uint32 Value
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

// static void Test_SendRoutingActivation(void);

// static void Test_SendUdsRequest(
//     const uint8* UdsPayloadPtr,
//     PduLengthType UdsPayloadLength
// );

// static void Test_MockFotaMaster_MainFunction(void);

// static void Test_InitModules(void);

// static void Test_MainFunctions(void);

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
//     UART_Printf("[GW] Gateway Mock FOTA Master Test Start\r\n");
//     UART_Printf("========================================\r\n");

//     while (1)
//     {
//         Test_MainFunctions();

//         Test_MockFotaMaster_MainFunction();

//         /*
//          * 실제 1ms tick 함수가 있으면 delay 대신 그걸 쓰는 게 좋다.
//          * 현재는 예시 구조이므로 프로젝트의 DelayMs 함수명에 맞춰 교체하면 된다.
//          */
//         /* Shared_DelayMs(TEST_MAIN_PERIOD_MS); */
//         Test_Tick1ms++;
//     }
// }

// /*********************************************************************************************************************/
// /*------------------------------------------------Module Init/Main---------------------------------------------------*/
// /*********************************************************************************************************************/

// static void Test_InitModules(void)
// {
//     UART_Init();

//     Can_Init();
//     CanIf_Init();
//     CanTp_Init();
//     PduR_Init();

//     /*
//      * Gateway는 TCP/DoIP endpoint이므로 DoIP도 초기화한다.
//      * SoAd/LwIP Init이 별도로 있다면 여기 앞단에 추가하면 된다.
//      */
//     DoIP_Init();

//     /*
//      * SoAd_Init();
//      * LwIP_Init();
//      */
// }

// static void Test_MainFunctions(void)
// {
//     /*
//      * 프로젝트에 실제 존재하는 MainFunction 이름에 맞춰 조정하면 된다.
//      * 핵심은 Gateway에서 CanTp 송수신 상태머신이 계속 돌아야 한다는 점이다.
//      */

//     Can_MainFunction_Read();
//     CanTp_MainFunction();
//     Can_MainFunction_Write();

//     /*
//      * SoAd/LwIP를 실제 TCP로 같이 돌릴 경우 추가.
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
//     static uint32 LastSendTick = 0U;

//     if ((Test_Tick1ms - LastSendTick) < TEST_SEND_PERIOD_MS)
//     {
//         return;
//     }

//     LastSendTick = Test_Tick1ms;

//     /*
//      * DoIP는 Diagnostic Message 전 Routing Activation이 필요하므로
//      * 가장 먼저 Routing Activation Request를 1회 주입한다.
//      */
//     if (Test_RoutingActivationSent == FALSE)
//     {
//         Test_SendRoutingActivation();
//         Test_RoutingActivationSent = TRUE;
//         return;
//     }

//     /*
//      * 이후 UDS Request를 하나씩 Gateway의 DoIP Rx 경로에 주입한다.
//      *
//      * Target ECU에서는:
//      * DoIP address field 제거된 순수 UDS payload만 DCM까지 도달해야 한다.
//      */
//     switch (Test_Step)
//     {
//         case 0U:
//         {
//             /*
//              * DiagnosticSessionControl - Extended Session
//              * Request: 10 03
//              * Expected Target Response: 50 03 ...
//              */
//             const uint8 UdsRequest[] = { 0x10U, 0x03U };

//             UART_Printf("[MOCK] Send UDS: 10 03 - Extended Session\r\n");

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_Step++;
//             break;
//         }

//         case 1U:
//         {
//             /*
//              * RequestDownload
//              *
//              * Request:
//              * 34 00 44 00 00 00 00 00 00 10 00
//              *
//              * 단순 테스트용.
//              */
//             const uint8 UdsRequest[] =
//             {
//                 0x34U, 0x00U, 0x44U,
//                 0x00U, 0x00U, 0x00U, 0x00U,
//                 0x00U, 0x00U, 0x10U, 0x00U
//             };

//             UART_Printf("[MOCK] Send UDS: 34 - RequestDownload\r\n");

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_Step++;
//             break;
//         }

//         case 2U:
//         {
//             /*
//              * TransferData block 1
//              * Request: 36 01 AA BB CC DD
//              */
//             const uint8 UdsRequest[] =
//             {
//                 0x36U, 0x01U,
//                 0xAAU, 0xBBU, 0xCCU, 0xDDU
//             };

//             UART_Printf("[MOCK] Send UDS: 36 01 - TransferData\r\n");

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_Step++;
//             break;
//         }

//         case 3U:
//         {
//             /*
//              * RequestTransferExit
//              * Request: 37
//              */
//             const uint8 UdsRequest[] = { 0x37U };

//             UART_Printf("[MOCK] Send UDS: 37 - RequestTransferExit\r\n");

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_Step++;
//             break;
//         }

//         case 4U:
//         {
//             /*
//              * RoutineControl - Verify Image
//              * Request: 31 01 FF 00
//              */
//             const uint8 UdsRequest[] =
//             {
//                 0x31U, 0x01U, 0xFFU, 0x00U
//             };

//             UART_Printf("[MOCK] Send UDS: 31 01 FF 00 - Verify Image\r\n");

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_Step++;
//             break;
//         }

//         case 5U:
//         {
//             /*
//              * DiagnosticSessionControl - Programming Session
//              * Request: 10 02
//              */
//             const uint8 UdsRequest[] = { 0x10U, 0x02U };

//             UART_Printf("[MOCK] Send UDS: 10 02 - Programming Session\r\n");

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_Step++;
//             break;
//         }

//         case 6U:
//         {
//             /*
//              * RoutineControl - Activate Image
//              * Request: 31 01 FF 01
//              */
//             const uint8 UdsRequest[] =
//             {
//                 0x31U, 0x01U, 0xFFU, 0x01U
//             };

//             UART_Printf("[MOCK] Send UDS: 31 01 FF 01 - Activate Image\r\n");

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_Step++;
//             break;
//         }

//         case 7U:
//         {
//             /*
//              * ECUReset - Hard Reset
//              * Request: 11 01
//              */
//             const uint8 UdsRequest[] = { 0x11U, 0x01U };

//             UART_Printf("[MOCK] Send UDS: 11 01 - ECU Reset\r\n");

//             Test_SendUdsRequest(
//                 UdsRequest,
//                 sizeof(UdsRequest)
//             );

//             Test_Step++;
//             break;
//         }

//         default:
//         {
//             /*
//              * 전체 시퀀스 1회 수행 후 정지.
//              * 반복 테스트를 원하면 Test_Step = 0U로 되돌리면 된다.
//              */
//             break;
//         }
//     }
// }

// static void Test_SendRoutingActivation(void)
// {
//     PduLengthType PacketLength;

//     PacketLength = Test_BuildRoutingActivationRequest(
//         Test_DoIPPacket
//     );

//     UART_Printf("[MOCK] Send DoIP Routing Activation Request\r\n");

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

//     PduInfo.SduDataPtr = (uint8*)PacketPtr;
//     PduInfo.SduLength  = PacketLength;

//     /*
//      * 실제 TCP 수신 대신, SoAd가 DoIP에 올려주는 상황을 직접 만든다.
//      *
//      * Mock FOTA Master
//      * -> DoIP_TpRxIndication()
//      * -> DoIP
//      * -> PduR
//      * -> CanTp
//      */
//     DoIP_TpRxIndication(
//         SOAD_RXPDU_DOIP_TCP,
//         &PduInfo
//     );
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

//     /*
//      * 현재 DoIP_HandleRoutingActivation()은 최소 3 bytes만 보고,
//      * Payload[0..1]을 TesterLogicalAddress로 사용한다.
//      */
//     Test_WriteUint16BigEndian(
//         &PayloadPtr[0],
//         TEST_TESTER_LOGICAL_ADDRESS
//     );

//     PayloadPtr[2] = 0x00U;

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

//     /*
//      * DoIP Diagnostic Message Payload:
//      *
//      * Byte 0~1 : SourceAddress = Tester
//      * Byte 2~3 : TargetAddress = Target ECU
//      * Byte 4~  : UDS Payload
//      */
//     PayloadLength = (uint32)UdsPayloadLength + 4U;

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