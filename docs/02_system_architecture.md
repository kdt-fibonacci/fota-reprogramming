# 02. System Architecture

## 관련 문서

- [Wiki Home](./README.md)
- [Project Overview](./00_project_overview.md)
- [Gateway ECU Architecture](./03_gateway_ecu_architecture.md)
- [DoIP Gateway Routing](./10_doip_gateway_routing.md)
- [FOTA Update Flow](./15_fota_update_flow.md)

## 1. 목적

5개 노드가 어떻게 연결되고, 진단 요청/응답과 FOTA 이미지가 어떤 네트워크/계층을 통해 흐르는지 시스템 레벨로 설명한다.

## 2. 범위

노드 간 네트워크 구간, 프로토콜 계층, diagnostic request/response/FOTA 경로의 큰 그림. 함수 수준 세부는 각 주제 문서로 위임한다.

## 3. 요약

- 서버↔마스터는 **TCP/IP (HTTP + MQTT)**, 마스터↔게이트웨이는 **DoIP/Ethernet(:13400)**, 게이트웨이↔타깃은 **CAN FD + CanTp**.
- Gateway는 DoIP logical address를 내부 PDU ID로 변환하고, PduR는 PDU ID 기반으로 CanTp 경로를 선택한다.
- Target ECU만 Dcm/FOTA/Flash를 수행한다.

## 4. 시스템 Context Diagram

```mermaid
flowchart LR
    subgraph SRV["FOTA_Linux_Server (192.168.203.16)"]
        H["HTTP :4321 (libmicrohttpd)"]
        Q["MQTT broker :1883"]
        DB["Flask dashboard :5000"]
    end
    subgraph RPI["FOTA_Master_RPI4 (DoIP Tester SA=0x0E00)"]
        W["ota_worker_thread"]
        E["UDS engine"]
        L["LCD UI thread"]
    end
    subgraph GW["Ecu_Gateway_TC375_LK (192.168.1.20)"]
        SO["SoAd (LwIP TCP)"]
        DP["DoIP"]
        PR["PduR"]
        CT["CanTp"]
    end
    MOT["Ecu_Motion_TC375_LK (LA 0x1234)"]
    LIT["Ecu_Lighting_TC375_LK (LA 0x5678)"]

    H <-->|HTTP| W
    Q -.->|push| W
    W --> E
    E <-->|DoIP :13400| SO
    SO --> DP --> PR --> CT
    CT <-->|CAN FD| MOT
    CT <-->|CAN FD| LIT
```

## 5. Layered Architecture (계층 구조)

```mermaid
flowchart TD
    subgraph Tester["RPI Master (Tester)"]
        TA["UDS engine"]
        TB["DoIP client"]
        TC["TCP socket"]
    end
    subgraph Gateway["Gateway ECU"]
        GA["SoAd (TCP)"]
        GB["DoIP"]
        GC["PduR"]
        GD["CanTp"]
        GE["CanIf"]
        GF["Can (CAN FD)"]
    end
    subgraph Target["Target ECU"]
        HA["Can (CAN FD)"]
        HB["CanIf"]
        HC["CanTp"]
        HD["PduR"]
        HE["Dcm (UDS)"]
        HF["FOTA Handler / Flash / Boot"]
    end
    TC --- GA
    GA --> GB --> GC --> GD --> GE --> GF
    GF === HA
    HA --> HB --> HC --> HD --> HE --> HF
```

근거:
- Gateway 계층 init 순서: `Test_InitModules()` in `Ecu_Gateway_TC375_LK/Cpu0_Main.c` (Can→CanIf→CanTp→PduR→LwIP→DoIP→SoAd→Dcm)
- Target 계층 init 순서: `Test_InitModules()` in `Ecu_Motion_TC375_LK/Cpu0_Main.c` (Can→CanIf→CanTp→PduR→Dcm→FOTA)

## 6. Diagnostic Request 경로 (overview)

```mermaid
sequenceDiagram
    participant M as RPI Master
    participant SO as Gateway SoAd
    participant DP as Gateway DoIP
    participant PR as Gateway PduR
    participant CT as Gateway CanTp
    participant T as Target (CanTp→PduR→Dcm)

    M->>SO: DoIP Diagnostic Message (SA,TA,UDS)
    SO->>DP: DoIP_TpRxIndication
    DP->>DP: TargetAddress → DoIPRxPduId 변환
    DP->>PR: PduR_DoIPTpRxIndication(DoIPRxPduId)
    PR->>CT: CanTp_Transmit(CANTP_TXNSDU_GATEWAY_TO_*)
    CT->>T: CAN FD SF/FF/CF
    T->>T: CanTp 재조립 → PduR → Dcm_RxIndication
```

## 7. Diagnostic Response 경로 (overview)

```mermaid
sequenceDiagram
    participant T as Target Dcm
    participant CT as Gateway CanTp
    participant PR as Gateway PduR
    participant DP as Gateway DoIP
    participant M as RPI Master

    T->>T: Dcm 응답 생성 → PduR_DcmTransmit → CanTp → CAN
    CT->>CT: CAN Rx 재조립 (Target→Gateway)
    CT->>PR: PduR_CanTpRxIndication(PDUR_RXPDU_CANTP_*_TO_TESTER)
    PR->>DP: DoIP_TpTransmit(DOIP_TXPDU_DIAG_RES_FROM_CANTP_*)
    DP->>DP: DoIPTxPduId → SourceAddress 결정, DoIP 메시지 래핑
    DP->>M: DoIP Diagnostic Message (SoAd_Transmit)
```

근거:
- `PduR_RoutingPathConfig[]` in `Ecu_Gateway_TC375_LK/Basics/ComStack/PduR/PduR_Cfg.c`
- `DoIP_TpTransmit()` in `Ecu_Gateway_TC375_LK/Basics/ComStack/DoIP/DoIP.c`

## 8. FOTA Image 경로 (overview)

```mermaid
sequenceDiagram
    participant S as Linux Server
    participant M as RPI Master
    participant G as Gateway
    participant T as Target

    S->>M: image .bin + .sig (HTTP GET /ota/down/...)
    M->>M: verifyFirmwareSecurity (ECDSA/SHA256)
    M->>G: 0x34 RequestDownload (image length)
    loop chunk (≤1024B)
        M->>G: 0x36 TransferData(sn, data)
        G->>T: CanTp 멀티프레임
        T->>T: SotaUpdate_WriteChunk → PFlash program
        T-->>M: positive response
    end
    M->>T: 0x37 TransferExit
    M->>T: 0x31 FF01 VerifyImage (CRC32)
    M->>T: 0x10 02 Programming Session (safe-state 대기)
    M->>T: 0x31 FF02 ActivateImage (UCB_SWAP arm)
    M->>T: 0x11 01 ECUReset (system reset)
```

근거:
- `startOtaTransfer()` in `FOTA_Master_RPI4/ota_uds_engine.cpp`
- Target 측 처리: `Dcm.c` 서비스 핸들러 + `FotaHandler.c`

## 9. Gateway와 Target 책임 분리

| 책임 | Gateway ECU | Target ECU (Motion/Lighting) |
|---|---|---|
| DoIP TCP 수신/Routing Activation | O | X |
| logical address → PDU ID 변환 | O (`DoIP.c`) | X |
| PduR 라우팅 | O (DoIP↔CanTp) | O (CanTp↔Dcm) |
| UDS 서비스 처리(Dcm) | X (local 진단 없음) | O |
| Flash erase/program/verify | X | O |
| Bank swap / reset | X | O |

> Gateway의 `Cpu0_Main.c`는 `Dcm_Init()`/`Dcm_MainFunction()`을 호출하지만, Gateway PduR 라우팅 테이블에는 Dcm 경로가 없다(`PduR_Cfg.c`의 4개 route 모두 DoIP↔CanTp). 따라서 Gateway Dcm은 현재 실질적으로 사용되지 않는다 — `확인 필요`. 자세히 [Diagnostic Gateway Management](./08_diagnostic_gateway_management.md).

## 10. Mermaid Diagram: 네트워크 구간 요약

```mermaid
flowchart LR
    A["Server"] -- "TCP/IP HTTP 4321 / MQTT 1883" --> B["Master"]
    B -- "Ethernet DoIP TCP 13400" --> C["Gateway"]
    C -- "CAN FD (Arbitration+Data BRS)" --> D["Motion/Lighting"]
```

## 11. 코드 근거

```text
근거:
- 계층 init: Test_InitModules() in Ecu_Gateway_TC375_LK/Cpu0_Main.c, Ecu_Motion_TC375_LK/Cpu0_Main.c
- DoIP 포트: SOAD_TCP_PORT_DOIP (13400U) in SoAd_Cfg.h
- 라우팅 테이블: PduR_RoutingPathConfig[] in PduR_Cfg.c (Gateway)
- DoIP 주소 변환: DoIP_FindRxPduConfigByTargetAddress(), DoIP_FindTxPduConfig() in DoIP.c
```

## 12. 추정 사항

- 서버→마스터 이미지가 차량 외부(클라우드/LAN)에서 전달된다는 표현은 IP 대역(192.168.203.x vs 192.168.1.x)으로 추정. 실제 물리 토폴로지는 `확인 필요`.

## 13. 확인 필요 사항

- Gateway local Dcm 사용 여부 (라우팅 미등록)
- DoIP ACK/NACK 비활성 정책

## 다음에 읽을 문서

- [Gateway ECU Architecture](./03_gateway_ecu_architecture.md)
- [DoIP Gateway Routing](./10_doip_gateway_routing.md)

## 이 문서에서 남은 확인 필요 사항

| 항목 | 내용 | 확인 방법 |
|---|---|---|
| 물리 네트워크 | 서버/마스터/게이트웨이 물리 연결 토폴로지 | 실제 배선/스위치 구성 확인 |
| Gateway Dcm | 게이트웨이 자체 진단 활성 여부 | `PduR_Cfg.c`에 Dcm route 추가 여부 추적 |
