# 23. Gateway Com Stack API/Type Specification

## 3. Gateway (Com Stack, Infineon AURIX TC375)

기준 코드: `Ecu_Gateway_TC375_LK/Basics/ComStack`, `Ecu_Gateway_TC375_LK/Basics/Common`, `Ecu_Gateway_TC375_LK/Cpu0_Main.c`

비고: 구조체 크기는 TC375 32-bit ABI 기준 추정값이다. `uint8/boolean=1B`, `uint16/PduIdType/PduLengthType=2B`, `uint32/pointer/enum=4B`, 자연 정렬을 가정했다. 컴파일러 enum packing 옵션이나 iLLD 타입 정의에 따라 달라질 수 있다.

### 3.1. Enum 명세

| Enum 이름 | Enum 설명 | 값 이름 | 값 | 값 설명 |
|---|---|---|---:|---|
| `Can_ControllerStateType` | CAN Controller 상태 | `CAN_CS_UNINIT` | `0x00` | 미초기화 |
| `Can_ControllerStateType` | CAN Controller 상태 | `CAN_CS_STARTED` | `0x01` | 송수신 가능 |
| `Can_ControllerStateType` | CAN Controller 상태 | `CAN_CS_STOPPED` | `0x02` | 정지 상태 |
| `Can_ControllerStateType` | CAN Controller 상태 | `CAN_CS_SLEEP` | `0x03` | Sleep 상태 |
| `Can_ObjectType` | CAN Hardware Object 방향 | `CAN_OBJECT_TYPE_TRANSMIT` | `0` | Tx HTH |
| `Can_ObjectType` | CAN Hardware Object 방향 | `CAN_OBJECT_TYPE_RECEIVE` | `1` | Rx HRH |
| `Can_RxDestinationType` | MCMCAN Rx 저장 위치 | `CAN_RX_DEST_FIFO0` | `0` | FIFO0 사용, 현재 구현 경로 |
| `Can_RxDestinationType` | MCMCAN Rx 저장 위치 | `CAN_RX_DEST_FIFO1` | `1` | 확장용 |
| `Can_RxDestinationType` | MCMCAN Rx 저장 위치 | `CAN_RX_DEST_BUFFER` | `2` | 전용 Rx Buffer 확장용 |
| `CanTp_TxStateType` | CanTp Tx 상태, 내부 | `CANTP_TX_STATE_IDLE` | `0` | 송신 유휴 |
| `CanTp_TxStateType` | CanTp Tx 상태, 내부 | `CANTP_TX_STATE_WAIT_FC` | `1` | First Frame 후 Flow Control 대기 |
| `CanTp_TxStateType` | CanTp Tx 상태, 내부 | `CANTP_TX_STATE_SEND_CF` | `2` | Consecutive Frame 송신 진행 |
| `CanTp_TxStateType` | CanTp Tx 상태, 내부 | `CANTP_TX_STATE_WAIT_TX_CONFIRMATION` | `3` | 하위 CanIf/Can Tx 확인 대기 |
| `CanTp_RxStateType` | CanTp Rx 상태, 내부 | `CANTP_RX_STATE_IDLE` | `0` | 수신 유휴 |
| `CanTp_RxStateType` | CanTp Rx 상태, 내부 | `CANTP_RX_STATE_RECEIVING` | `1` | 멀티프레임 재조립 중 |
| `PduR_ModuleType` | PduR route source/destination module | `PDUR_MODULE_DCM` | `0` | DCM |
| `PduR_ModuleType` | PduR route source/destination module | `PDUR_MODULE_CANTP` | `1` | CanTp |
| `PduR_ModuleType` | PduR route source/destination module | `PDUR_MODULE_DOIPTP` | `2` | DoIP transport |
| `PduR_EventType` | PduR route trigger | `PDUR_EVENT_TRANSMIT` | `0` | 상위 모듈 송신 요청 |
| `PduR_EventType` | PduR route trigger | `PDUR_EVENT_RX_INDICATION` | `1` | 수신 완료 indication |
| `PduR_EventType` | PduR route trigger | `PDUR_EVENT_TX_CONFIRMATION` | `2` | 송신 완료 confirmation |
| `DoIP_StateType` | DoIP runtime 상태, 내부 | `DOIP_STATE_INITIALIZED` | `0` | 초기화 완료, Routing Activation 전 |
| `DoIP_StateType` | DoIP runtime 상태, 내부 | `DOIP_STATE_ROUTING_ACTIVE` | `1` | Routing Activation 완료 |
| `SoAd_SoConStateType` | TCP Socket Connection 상태, 내부 | `SOAD_SOCON_STATE_OFFLINE` | `0` | 소켓 오프라인 |
| `SoAd_SoConStateType` | TCP Socket Connection 상태, 내부 | `SOAD_SOCON_STATE_LISTENING` | `1` | TCP listen 중 |
| `SoAd_SoConStateType` | TCP Socket Connection 상태, 내부 | `SOAD_SOCON_STATE_ONLINE` | `2` | TCP 연결 수립 |
| `SoAd_SoConStateType` | TCP Socket Connection 상태, 내부 | `SOAD_SOCON_STATE_CLOSING` | `3` | 연결 종료 처리 중 |
| `Dcm_OpStatusType` | DCM 비동기 callout 상태 | `DCM_OP_INITIAL` | `0` | 최초 호출 |
| `Dcm_OpStatusType` | DCM 비동기 callout 상태 | `DCM_OP_PENDING` | `1` | 처리 보류/재호출 |
| `Dcm_ReturnWriteMemoryType` | Memory Write 결과 | `DCM_WRITE_OK` | `0` | 성공 |
| `Dcm_ReturnWriteMemoryType` | Memory Write 결과 | `DCM_WRITE_PENDING` | `1` | 비동기 처리 중 |
| `Dcm_ReturnWriteMemoryType` | Memory Write 결과 | `DCM_WRITE_FAILED` | `2` | 실패 |
| `Dcm_SessionType` | UDS diagnostic session | `DCM_SESSION_DEFAULT` | `0x01` | Default Session |
| `Dcm_SessionType` | UDS diagnostic session | `DCM_SESSION_PROGRAMMING` | `0x02` | Programming Session |
| `Dcm_SessionType` | UDS diagnostic session | `DCM_SESSION_EXTENDED` | `0x03` | Extended Session |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_IDLE` | `0` | 대기 |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_EXTENDED_SESSION` | `1` | Extended session 진입 |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_DOWNLOAD_ACCEPTED` | `2` | RequestDownload 승인 |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_TRANSFER_IN_PROGRESS` | `3` | TransferData 진행 |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_TRANSFER_COMPLETED` | `4` | TransferExit 완료 |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_VERIFY_IN_PROGRESS` | `5` | 검증 진행 예약 상태 |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_VERIFIED` | `6` | 이미지 검증 완료 |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_PROGRAMMING_SESSION` | `7` | Programming session 진입 |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_ACTIVATION_PENDING` | `8` | 활성화 대기 |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_ACTIVATED` | `9` | 활성화 완료 상태 |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_ROLLBACK_PENDING` | `10` | 롤백 대기 |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_ROLLBACK_DONE` | `11` | 롤백 완료 |
| `Dcm_FotaStateType` | DCM FOTA 진행 상태 | `DCM_FOTA_STATE_FAILED` | `12` | 실패 |
| `Dcm_FotaResultType` | 마지막 FOTA 결과 | `DCM_FOTA_RESULT_NONE` | `0` | 결과 없음 |
| `Dcm_FotaResultType` | 마지막 FOTA 결과 | `DCM_FOTA_RESULT_SUCCESS` | `1` | 성공 |
| `Dcm_FotaResultType` | 마지막 FOTA 결과 | `DCM_FOTA_RESULT_DOWNLOAD_FAILED` | `2` | 다운로드 실패 |
| `Dcm_FotaResultType` | 마지막 FOTA 결과 | `DCM_FOTA_RESULT_TRANSFER_FAILED` | `3` | 전송 실패 |
| `Dcm_FotaResultType` | 마지막 FOTA 결과 | `DCM_FOTA_RESULT_VERIFY_FAILED` | `4` | 검증 실패 |
| `Dcm_FotaResultType` | 마지막 FOTA 결과 | `DCM_FOTA_RESULT_ACTIVATION_FAILED` | `5` | 활성화 실패 |
| `Dcm_FotaResultType` | 마지막 FOTA 결과 | `DCM_FOTA_RESULT_ROLLBACK_DONE` | `6` | 롤백 완료 |
| `Dcm_FotaResultType` | 마지막 FOTA 결과 | `DCM_FOTA_RESULT_FAILED` | `7` | 일반 실패 |
| `Dcm_RequestStateType` | DCM 요청 처리 상태, 내부 | `DCM_REQUEST_IDLE` | `0` | 처리할 요청 없음 |
| `Dcm_RequestStateType` | DCM 요청 처리 상태, 내부 | `DCM_REQUEST_PENDING` | `1` | `Dcm_MainFunction()` 처리 대기 |

### 3.2. 구조체 명세

| 구조체명 | 설명 | 크기(Byte) | 포함 멤버 | 정의 파일 |
|---|---|---:|---|---|
| `PduInfoType` | ComStack 공통 PDU 버퍼 descriptor | 12 | `SduDataPtr`, `SduLength`, `MetaDataPtr` | `Basics/Common/ComStack_Types.h` |
| `Can_PduType` | CAN Driver 송신 PDU | 16 | `swPduHandle`, `id`, `sdu`, `length` | `ComStack/Can/Can.h` |
| `Can_HwType` | CAN Rx mailbox 정보 | 8 | `CanId`, `Hoh`, `ControllerId` | `ComStack/Can/Can.h` |
| `Can_ControllerConfigType` | CAN controller static config | 3 | `CanControllerId`, `CanNodeId`, `CanFdEnabled` | `ComStack/Can/Can_Cfg.h` |
| `Can_TxObjectConfigType` | CAN Tx object 세부 설정 | 1 | `CanTxBufferIndex` | `ComStack/Can/Can_Cfg.h` |
| `Can_RxObjectConfigType` | CAN Rx object/filter 설정 | 16 | `CanRxDestination`, `CanFilterIndex`, `CanFilterId1`, `CanFilterId2` | `ComStack/Can/Can_Cfg.h` |
| `Can_HardwareObjectConfigType` | CAN HOH 설정, Tx/Rx union 포함 | 28 | `CanObjectId`, `CanObjectType`, `CanControllerId`, `CanObjectPayloadLength`, `ObjectConfig` | `ComStack/Can/Can_Cfg.h` |
| `CanIf_TxPduConfigType` | CanIf Tx L-PDU → CAN/CanTp 매핑 | 12 | `CanIfTxPduId`, `CanId`, `Hth`, `CanTpTxPduId` | `ComStack/CanIf/CanIf_Cfg.h` |
| `CanIf_RxPduConfigType` | CanIf Rx L-PDU → CanTp 매핑 | 12 | `CanIfRxPduId`, `Hrh`, `CanId`, `CanTpRxPduId` | `ComStack/CanIf/CanIf_Cfg.h` |
| `CanTp_TxNsduConfigType` | CanTp Tx N-SDU route/config | 8 | `CanTpTxNsduId`, `CanIfTxNpduId`, `PduRTxPduId`, `ExpectedRxNsduId` | `ComStack/CanTp/CanTp_Cfg.h` |
| `CanTp_RxNsduConfigType` | CanTp Rx N-SDU route/config | 12 | `CanTpRxNsduId`, `CanIfRxNpduId`, `CanIfTxFcPduId`, `PduRRxPduId`, `RxBufferSize`, `BlockSize`, `STmin` | `ComStack/CanTp/CanTp_Cfg.h` |
| `PduR_RoutingPathConfigType` | PDU Router route entry | 24 | `PduRRoutingPathId`, `SourceModule`, `SourcePduId`, `DestModule`, `DestPduId`, `RoutingEvent` | `ComStack/PduR/PduR_Cfg.h` |
| `DoIP_ConfigType` | DoIP entity/tester/SoAd PDU 설정 | 8 | `EntityLogicalAddress`, `TesterLogicalAddress`, `SoAdRxPduId`, `SoAdTxPduId` | `ComStack/DoIP/DoIP_Cfg.h` |
| `DoIP_RxPduConfigType` | DoIP TargetAddress → DoIPRxPduId 매핑 | 4 | `DoIPRxPduId`, `TargetAddress` | `ComStack/DoIP/DoIP_Cfg.h` |
| `DoIP_TxPduConfigType` | DoIPTxPduId → SourceAddress 매핑 | 4 | `DoIPTxPduId`, `SourceAddress` | `ComStack/DoIP/DoIP_Cfg.h` |
| `SoAd_SocketConnectionConfigType` | TCP socket connection 설정 | 8 | `SoConId`, `LocalPort`, `RxPduId`, `TxPduId` | `ComStack/SoAd/SoAd_Cfg.h` |
| `Can_TxPendingType` | CAN Tx pending runtime, 내부 | 6 | `IsPending`, `SwPduHandle`, `CanControllerId`, `CanTxBufferIndex` | `ComStack/Can/Can.c` |
| `Can_DriverRuntimeType` | iLLD CAN module/node runtime, 내부 | iLLD 의존 | `CanModule`, `Node[CAN_CONTROLLER_COUNT]` | `ComStack/Can/Can.c` |
| `CanTp_TxRuntimeType` | CanTp Tx segmentation runtime, 내부 | 2136 | 상태, PDU ID들, 길이/sequence/block 제어, `Buffer[2048]`, `CanFrameBuffer[64]` | `ComStack/CanTp/CanTp.c` |
| `CanTp_RxRuntimeType` | CanTp Rx reassembly runtime, 내부 | 2128 | 상태, PDU ID들, 길이/sequence/block 제어, `Buffer[2048]`, `FcFrameBuffer[64]` | `ComStack/CanTp/CanTp.c` |
| `DoIP_RuntimeType` | DoIP state/Rx/Tx buffer runtime, 내부 | 4108 | `State`, logical addresses, `RxBuffer[2048]`, `RxLength`, `TxBuffer[2048]` | `ComStack/DoIP/DoIP.c` |
| `SoAd_SocketConnectionRuntimeType` | TCP socket connection runtime, 내부 | 4120 | `IsUsed`, `State`, `SoConId`, `Retries`, `ListenPcb`, `ConnectionPcb`, Rx/Tx buffers | `ComStack/SoAd/SoAd.c` |
| `Dcm_RuntimeType` | DCM session/FOTA/request buffer runtime, 내부 | 4120 | session/FOTA/request 상태, Rx/Tx buffers, block sequence counter | `ComStack/Dcm/Dcm.c` |

### 3.3. 헤더 파일 명세

| 헤더 파일 | 공개 타입 | 공개 함수 | 사용자 모듈 |
|---|---|---|---|
| `Basics/Common/Std_Types.h` | `Std_ReturnType`, `E_OK`, `E_NOT_OK`, `CAN_BUSY`, `TRUE/FALSE`, `NULL_PTR` | 없음 | 전체 ComStack |
| `Basics/Common/ComStack_Types.h` | `PduIdType`, `PduLengthType`, `PduInfoType` | 없음 | Can/CanIf/CanTp/PduR/Dcm/DoIP/SoAd |
| `ComStack/Can/Can.h` | `Can_IdType`, `Can_HwHandleType`, `Can_ControllerStateType`, `Can_PduType`, `Can_HwType` | `Can_Init`, `Can_SetControllerMode`, `Can_GetControllerMode`, `Can_Write`, `Can_MainFunction_Read`, `Can_MainFunction_Write` | `Cpu0_Main.c`, `CanIf.c` |
| `ComStack/Can/Can_Cfg.h` | `Can_ObjectType`, `Can_RxDestinationType`, CAN config structs | 없음, extern config 제공 | `Can.c`, `CanIf_Cfg.c`, `Cpu0_Main.c` |
| `ComStack/CanIf/CanIf.h` | 없음 | `CanIf_Init`, `CanIf_Transmit`, `CanIf_RxIndication`, `CanIf_TxConfirmation` | `Can.c`, `CanTp.c`, `Cpu0_Main.c` |
| `ComStack/CanIf/CanIf_Cfg.h` | `CanIf_TxPduConfigType`, `CanIf_RxPduConfigType` | 없음, extern config 제공 | `CanIf.c`, `CanTp_Cfg.c` |
| `ComStack/CanTp/CanTp.h` | 없음 | `CanTp_Init`, `CanTp_Transmit`, `CanTp_RxIndication`, `CanTp_TxConfirmation`, `CanTp_MainFunction` | `PduR.c`, `CanIf.c`, `Cpu0_Main.c` |
| `ComStack/CanTp/CanTp_Cfg.h` | `CanTp_TxNsduConfigType`, `CanTp_RxNsduConfigType` | 없음, extern config 제공 | `CanTp.c`, `PduR_Cfg.c` |
| `ComStack/PduR/PduR.h` | 없음 | `PduR_Init`, `PduR_DcmTransmit`, `PduR_DoIPTpRxIndication`, `PduR_DoIPTpTxConfirmation`, `PduR_CanTpRxIndication`, `PduR_CanTpTxConfirmation` | `DoIP.c`, `CanTp.c`, `Dcm.c`, `Cpu0_Main.c` |
| `ComStack/PduR/PduR_Cfg.h` | `PduR_ModuleType`, `PduR_EventType`, `PduR_RoutingPathConfigType` | 없음, extern config 제공 | `PduR.c`, `DoIP_Cfg.c`, `CanTp_Cfg.c` |
| `ComStack/DoIP/DoIP.h` | 없음 | `DoIP_Init`, `DoIP_TpRxIndication`, `DoIP_TpTransmit` | `SoAd.c`, `PduR.c`, `Cpu0_Main.c` |
| `ComStack/DoIP/DoIP_Cfg.h` | `DoIP_ConfigType`, `DoIP_RxPduConfigType`, `DoIP_TxPduConfigType` | 없음, extern config 제공 | `DoIP.c`, `PduR_Cfg.c` |
| `ComStack/SoAd/SoAd.h` | 없음 | `SoAd_Init`, `SoAd_Transmit` | `DoIP.c`, `Cpu0_Main.c` |
| `ComStack/SoAd/SoAd_Cfg.h` | `SoAd_SocketConnectionConfigType` | 없음, extern config 제공 | `SoAd.c`, `DoIP_Cfg.c` |
| `ComStack/LwIP/LwIP.h` | 없음 | `LwIP_Init`, `LwIP_MainFunction`, `LwIP_TimerInit` | `Cpu0_Main.c` |
| `ComStack/LwIP/LwIP_Cfg.h` | `EthAddr` extern, MAC 주소 매크로 | 없음 | `LwIP.c`, `LwIP_Cfg.c` |
| `ComStack/Dcm/Dcm.h` | `Dcm_OpStatusType`, `Dcm_ReturnWriteMemoryType`, `Dcm_SessionType`, `Dcm_FotaStateType`, `Dcm_FotaResultType` | `Dcm_Init`, `Dcm_RxIndication`, `Dcm_TxConfirmation`, `Dcm_MainFunction`, `Dcm_GetCurrentSession`, `Dcm_GetFotaState`, `Dcm_GetLastFotaResult` | `PduR.c`, `Cpu0_Main.c` |
| `ComStack/Dcm/Dcm_Cfg.h` | 없음, UDS SID/NRC/PDU/RID 매크로 | 없음 | `Dcm.c` |

### 3.4. 함수 상세 명세

| 소스 파일 | 함수명 | 상세 내용 | 입력 파라미터 | 반환값 | 비고 |
|---|---|---|---|---|---|
| `Cpu0_Main.c` | `Test_InitModules` | Gateway ComStack 초기화 순서 수행: UART, Can, controller start, CanIf, CanTp, PduR, LwIP, DoIP, SoAd, Dcm | 없음 | 없음 | `static`; Gateway bring-up 순서의 기준 |
| `Cpu0_Main.c` | `Test_MainFunctions` | 1ms cooperative loop에서 CAN Rx, CanTp, CAN Tx, LwIP, Dcm 주기 처리 | 없음 | 없음 | `static`; `Dcm_MainFunction`은 route 부재로 실질 미사용 추정 |
| `Can.c` | `Can_Init` | CAN0 node0, CAN FD 64B, 500 kbps/2 Mbps, Rx FIFO0, Tx buffer, standard filter 초기화 | 없음 | 없음 | controller state는 STOPPED로 시작 |
| `Can.c` | `Can_SetControllerMode` | controller 상태 전이 검증 및 설정 | `Controller`, `Transition` | `E_OK`/`E_NOT_OK` | Gateway는 init 후 `CAN_CS_STARTED`로 전이 |
| `Can.c` | `Can_GetControllerMode` | 현재 controller 상태 반환 | `Controller`, `ControllerModePtr` | `E_OK`/`E_NOT_OK` | null pointer/범위 검사 |
| `Can.c` | `Can_Write` | HTH와 `Can_PduType` 검증 후 iLLD `IfxCan_Can_sendMessage` 호출, pending 등록 | `Hth`, `PduInfoPtr` | `E_OK`/`E_NOT_OK` | Tx 완료는 `Can_MainFunction_Write`에서 확인 |
| `Can.c` | `Can_MainFunction_Read` | Rx FIFO0를 polling하여 CAN ID로 HRH를 찾고 `CanIf_RxIndication` 호출 | 없음 | 없음 | Gateway main loop에서 1ms마다 호출 |
| `Can.c` | `Can_MainFunction_Write` | pending Tx buffer 완료 여부 확인 후 `CanIf_TxConfirmation` 호출 | 없음 | 없음 | interrupt 대신 polling confirmation |
| `CanIf.c` | `CanIf_Init` | 현재 runtime 상태 없음 | 없음 | 없음 | 향후 PDU mode/controller mode 확장 지점 |
| `CanIf.c` | `CanIf_Transmit` | CanIf Tx PDU를 CAN ID/HTH로 매핑해 `Can_Write` 호출 | `CanIfTxPduId`, `PduInfoPtr` | `E_OK`/`E_NOT_OK` | 상위는 CanTp |
| `CanIf.c` | `CanIf_RxIndication` | `Hoh+CanId`로 Rx config를 찾아 `CanTp_RxIndication` 호출 | `Mailbox`, `PduInfoPtr` | 없음 | CanIf Rx 상위는 CanTp로 고정 |
| `CanIf.c` | `CanIf_TxConfirmation` | CanIf Tx PDU를 CanTp Tx N-PDU로 변환해 `CanTp_TxConfirmation` 호출 | `CanIfTxPduId`, `result` | 없음 | Can Driver 결과를 bypass 전달 |
| `CanTp.c` | `CanTp_Init` | Tx/Rx runtime 배열 초기화 | 없음 | 없음 | Tx/Rx state를 IDLE로 reset |
| `CanTp.c` | `CanTp_Transmit` | UDS N-SDU 길이에 따라 Single Frame 또는 First Frame 송신 시작 | `CanTpTxSduId`, `CanTpTxInfoPtr` | `E_OK`/`E_NOT_OK` | 멀티프레임은 FC 수신 후 CF 진행 |
| `CanTp.c` | `CanTp_RxIndication` | CAN frame PCI type(SF/FF/CF/FC)을 판별해 처리 함수로 dispatch | `CanTpRxNPduId`, `PduInfoPtr` | 없음 | 완성 N-SDU는 `PduR_CanTpRxIndication`으로 전달 |
| `CanTp.c` | `CanTp_TxConfirmation` | 하위 N-PDU 송신 결과에 따라 다음 상태 결정 또는 PduR confirmation 전달 | `CanTpTxNPduId`, `result` | 없음 | SF 완료/마지막 CF 완료 시 PduR에 알림 |
| `CanTp.c` | `CanTp_MainFunction` | `SEND_CF` 상태 Tx runtime을 진행해 CF 송신 | 없음 | 없음 | polling 기반 segmentation 진행 |
| `PduR.c` | `PduR_Init` | PduR initialized flag 설정 | 없음 | 없음 | route table은 compile-time config |
| `PduR.c` | `PduR_DcmTransmit` | DCM 송신 요청 route 검색 후 목적 모듈로 전달 | `DcmTxPduId`, `PduInfoPtr` | `E_OK`/`E_NOT_OK` | Gateway route table에는 DCM route 없음 |
| `PduR.c` | `PduR_DoIPTpRxIndication` | DoIP Rx PDU ID로 route 검색 후 CanTp/DoIP/DCM 목적지 호출 | `DoIPRxPduId`, `PduInfoPtr` | 없음 | Gateway Tester→Motion/Lighting 핵심 경로 |
| `PduR.c` | `PduR_DoIPTpTxConfirmation` | DoIP Tx confirmation route 검색 및 전달 | `DoIPTxPduId`, `Result` | 없음 | Gateway table에 confirmation route 없음 |
| `PduR.c` | `PduR_CanTpRxIndication` | CanTp 수신 완료 PDU를 DoIP Tx PDU로 route | `PduRRxPduId`, `PduInfoPtr` | 없음 | Target response→Tester 핵심 경로 |
| `PduR.c` | `PduR_CanTpTxConfirmation` | CanTp Tx confirmation route 검색 및 전달 | `PduRTxPduId`, `Result` | 없음 | Gateway table에 confirmation route 없음 |
| `DoIP.c` | `DoIP_Init` | DoIP runtime clear, state INITIALIZED, tester/entity logical address 설정 | 없음 | 없음 | 기본 tester `0x0E00`, entity `0x0F00` |
| `DoIP.c` | `DoIP_TpRxIndication` | SoAd TCP payload를 Rx buffer에 누적하고 DoIP message parser 실행 | `SoAdRxPduId`, `PduInfoPtr` | 없음 | overflow 시 Generic NACK 후 Rx reset |
| `DoIP.c` | `DoIP_TpTransmit` | PduR에서 받은 UDS 응답을 DoIP Diagnostic Message로 래핑해 `SoAd_Transmit` | `DoIPTxPduId`, `PduInfoPtr` | `E_OK`/`E_NOT_OK` | Routing Active 상태에서만 송신 |
| `DoIP.c` | `DoIP_ProcessRxBuffer` | TCP stream에서 DoIP header/payload length 기준으로 메시지 경계 처리 | 없음 | 없음 | `static`; partial TCP frame 처리 |
| `DoIP.c` | `DoIP_HandleRoutingActivation` | tester logical address 저장, state ROUTING_ACTIVE, success response 송신 | `PayloadPtr`, `PayloadLength` | 없음 | payload 길이 3 미만이면 Generic NACK |
| `DoIP.c` | `DoIP_HandleDiagnosticMessage` | SA/TA/UDS 분리, TA→DoIPRxPduId 변환, `PduR_DoIPTpRxIndication` 호출 | `PayloadPtr`, `PayloadLength` | 없음 | Gateway local DCM 미지원, unknown target은 return |
| `SoAd.c` | `SoAd_Init` | LwIP TCP PCB 생성, port 13400 bind/listen, accept callback 등록 | 없음 | 없음 | `SOAD_SOCKET_CONNECTION_COUNT=1` |
| `SoAd.c` | `SoAd_Transmit` | online TCP connection에 `tcp_write`/`tcp_output`으로 PDU 송신 | `SoAdTxPduId`, `PduInfoPtr` | `E_OK`/`E_NOT_OK` | send buffer 부족/connection 없음이면 실패 |
| `SoAd.c` | `SoAd_Accept` | TCP accept callback, connection runtime ONLINE 설정 및 recv/sent/poll/error callback 등록 | `Arg`, `NewPcb`, `Error` | `err_t` | `static`; 단일 connection runtime 사용 |
| `SoAd.c` | `SoAd_Recv` | pbuf chain을 RxBuffer로 복사 후 `DoIP_TpRxIndication` 호출 | `Arg`, `TcpPcb`, `Pbuf`, `Error` | `err_t` | `static`; TCP close 시 `SoAd_Close` |
| `LwIP.c` | `LwIP_TimerInit` | STM compare interrupt 설정 | 없음 | 없음 | `ISR_PRIORITY_OS_TICK`, CPU0 |
| `LwIP.c` | `LwIP_Init` | GETH enable, timer init, `Ifx_Lwip_init(EthAddr)` 호출 | 없음 | 없음 | MAC은 `LwIP_Cfg.c` |
| `LwIP.c` | `LwIP_MainFunction` | LwIP timer/receive flag polling | 없음 | 없음 | Gateway main loop에서 호출 |
| `LwIP.c` | `updateLwIPStackISR` | STM compare 갱신, `g_TickCount_1ms++`, `Ifx_Lwip_onTimerTick` 호출 | 없음 | 없음 | interrupt handler |
| `Dcm.c` | `Dcm_Init` | session/FOTA/request runtime 초기화 | 없음 | 없음 | Gateway에서는 route 부재로 local diagnostic 미사용 추정 |
| `Dcm.c` | `Dcm_RxIndication` | DCM Rx PDU 검증 후 UDS request를 내부 RxBuffer에 복사하고 pending 설정 | `DcmRxPduId`, `PduInfoPtr` | 없음 | Gateway PduR table에는 DCM Rx route 없음 |
| `Dcm.c` | `Dcm_TxConfirmation` | DCM Tx 결과 반영, 실패 시 FOTA result/state 갱신 | `DcmTxPduId`, `Result` | 없음 | Gateway에서는 일반 경로 미사용 |
| `Dcm.c` | `Dcm_MainFunction` | pending request가 있으면 UDS service dispatch 수행 | 없음 | 없음 | Gateway main loop에서 호출되지만 요청 유입 없음 |
| `Dcm.c` | `Dcm_GetCurrentSession` | 현재 session 반환 | 없음 | `Dcm_SessionType` | 상태 조회 API |
| `Dcm.c` | `Dcm_GetFotaState` | 현재 FOTA 상태 반환 | 없음 | `Dcm_FotaStateType` | 상태 조회 API |
| `Dcm.c` | `Dcm_GetLastFotaResult` | 마지막 FOTA 결과 반환 | 없음 | `Dcm_FotaResultType` | 상태 조회 API |

## Gateway route 요약

| 방향 | Source | Destination | Route |
|---|---|---|---|
| Tester → Motion | DoIP `DOIP_RXPDU_DIAG_REQ_TO_CANTP_MOTION` | CanTp `CANTP_TXNSDU_GATEWAY_TO_MOTION` | `PDUR_ROUTE_DOIP_TO_CANTP_TESTER_TO_MOTION` |
| Tester → Lighting | DoIP `DOIP_RXPDU_DIAG_REQ_TO_CANTP_LIGHTING` | CanTp `CANTP_TXNSDU_GATEWAY_TO_LIGHTING` | `PDUR_ROUTE_DOIP_TO_CANTP_TESTER_TO_LIGHTING` |
| Motion → Tester | CanTp `PDUR_RXPDU_CANTP_MOTION_TO_TESTER` | DoIP `DOIP_TXPDU_DIAG_RES_FROM_CANTP_MOTION` | `PDUR_ROUTE_CANTP_TO_DOIP_MOTION_TO_TESTER` |
| Lighting → Tester | CanTp `PDUR_RXPDU_CANTP_LIGHTING_TO_TESTER` | DoIP `DOIP_TXPDU_DIAG_RES_FROM_CANTP_LIGHTING` | `PDUR_ROUTE_CANTP_TO_DOIP_LIGHTING_TO_TESTER` |
