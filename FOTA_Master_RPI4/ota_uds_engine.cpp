// ota_uds_engine.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <iomanip>
#include <thread> 

#include "state.h"

extern STATE current_state;
extern int current_install_progress;
extern bool isRecoveryGo;

const int DOIP_PORT = 13400;
const uint16_t RPI_SA = 0x0E00; 

// 💡 바이너리 파일 수신 시 타겟 제어기가 플래싱을 시작할 베이스 주소 정의 (프로젝트 사양에 맞게 수정 가능)
const uint32_t FLASH_START_ADDRESS = 0x80000000; 

uint8_t hstob(const std::string& hex) { return (uint8_t)std::stoul(hex, nullptr, 16); }

uint32_t calculateChunkCRC32(const std::vector<uint8_t>& data) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; i++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc; 
}

int recv_with_retry(int sock, uint8_t* buf, int max_len) {
    int retries = 20; 
    while (retries > 0) {
        int rLen = recv(sock, buf, max_len, 0);
        if (rLen > 0) return rLen; 
        
        if (rLen == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            retries--;
            std::cout << "Retries: " << retries << '\n';
        } else {
            return -1; 
        }
    }
    return -1; 
}

// 💡 [수정 포인트 1]: 라즈베리파이가 송신(TX)하는 raw 패킷을 Hex 형태로 완벽하게 시각화 로그 출력
void sendUdsPacket(int sock, uint16_t targetAddr, uint8_t sid, const std::vector<uint8_t>& payload) {
    uint32_t udsLen = payload.size() + 1;
    uint32_t doipPayloadLen = udsLen + 4;
    std::vector<uint8_t> pkt;
    pkt.push_back(0x02); pkt.push_back(0xFD);
    pkt.push_back(0x80); pkt.push_back(0x01);
    pkt.push_back(0x00); pkt.push_back(0x00);
    pkt.push_back((doipPayloadLen >> 8) & 0xFF); pkt.push_back(doipPayloadLen & 0xFF);
    pkt.push_back((RPI_SA >> 8) & 0xFF); pkt.push_back(RPI_SA & 0xFF);
    pkt.push_back((targetAddr >> 8) & 0xFF); pkt.push_back(targetAddr & 0xFF);
    pkt.push_back(sid);
    pkt.insert(pkt.end(), payload.begin(), payload.end());

    // ----------------------------------------------------
    // 💡 RAW TX 패킷 터미널 Hex 로깅 시스템 안착
    // ----------------------------------------------------
    std::cout << "[DoIP][TX] Raw Packet (Len=" << pkt.size() << "): ";
    std::ios_base::fmtflags f(std::cout.flags()); // 기존 std::cout 포맷 백업
    for (uint8_t byte : pkt) {
        std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)byte << " ";
    }
    std::cout << std::endl;
    std::cout.flags(f); // 기존 std::cout 포맷 복원
    // ----------------------------------------------------

    send(sock, pkt.data(), pkt.size(), 0);
}

int checkUdsResponse(uint8_t* res, int len, uint8_t expectedPositiveSid, uint16_t expectedRid = 0) {
    std::cout << "[DoIP][RX] Raw Packet (Len=" << len << "): ";
    std::ios_base::fmtflags f(std::cout.flags()); 
    for (int i = 0; i < len; ++i) {
        std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)res[i] << " ";
    }
    std::cout << std::endl;
    std::cout.flags(f); 

    int doip_offset = -1;
    for (int i = 0; i <= len - 8; i++) {
        if (res[i] == 0x02 && res[i+1] == 0xFD && res[i+2] == 0x80 && res[i+3] == 0x01) {
            doip_offset = i;
            break;
        }
    }

    if (doip_offset == -1) {
        std::cerr << "[DEBUG ERROR] DoIP Diagnostic Message Header not found in receive buffer." << std::endl;
        return -100; 
    }

    int sid_index = doip_offset + 12;
    if (sid_index >= len) return -100;

    uint8_t sid = res[sid_index];

    if (sid == expectedPositiveSid) {
        if (expectedRid != 0) {
            uint16_t receivedRid = (res[sid_index + 2] << 8) | res[sid_index + 3];
            if (receivedRid != expectedRid) return -101;
        }
        return 0;
    } 
    else if (sid == 0x7F) {
        uint8_t nrc = res[sid_index + 2];
        if (nrc == 0x78) return 0x78;
        return nrc;
    }

    std::cout << "⚠️ [MISMATCH] Expected SID: 0x" << std::hex << (int)expectedPositiveSid 
              << ", Detected SID: 0x" << (int)sid << std::dec << " at index " << sid_index << std::endl;

    return -102;
}

bool routingActivation(int sock) {
    uint8_t actReq[11] = {0x02, 0xFD, 0x00, 0x05, 0x00, 0x00, 0x00, 0x03, 0x0E, 0x00, 0x00};
    
    // routingActivation은 기존 하드코딩 send 구조이므로 가독성을 위해 직접 출력 바인딩
    std::cout << "[DoIP][TX] Raw Packet (Len=11): 02 fd 00 05 00 00 00 03 0e 00 00" << std::endl;
    send(sock, actReq, sizeof(actReq), 0);
    
    uint8_t res[32];
    int len = recv_with_retry(sock, res, sizeof(res));
    return (len >= 8 && res[2] == 0x00 && res[3] == 0x06);
}

int enterProgrammingSession(int sock, uint16_t targetAddr) {
    std::cout << "[UDS] Entering Programming Session (0x10 03)..." << std::endl;
    sendUdsPacket(sock, targetAddr, 0x10, {0x03});
    uint8_t res[64];
    int len = recv_with_retry(sock, res, sizeof(res));
    return checkUdsResponse(res, len, 0x50);
}

int changeDiagnosticSession(int sock, uint16_t targetAddr, uint8_t sessionType) {
    std::string sessionName = (sessionType == 0x02) ? "Programming Session (0x02)" : "Extended Session (0x03)";
    std::cout << "[UDS] Requesting Session Switch to " << sessionName << "..." << std::endl;
    
    sendUdsPacket(sock, targetAddr, 0x10, { sessionType });
    uint8_t res[64];
    int len = recv_with_retry(sock, res, sizeof(res));
    return checkUdsResponse(res, len, 0x50);
}

int requestDownload(int sock, uint16_t targetAddr, uint32_t addr, uint32_t size) {
    std::cout << "[UDS] Requesting Download (0x34) to Addr: 0x" << std::hex << addr << std::dec << std::endl;
    std::vector<uint8_t> p = { 0x00, 0x44, (uint8_t)(addr >> 24), (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr, (uint8_t)(size >> 24), (uint8_t)(size >> 16), (uint8_t)(size >> 8), (uint8_t)size };
    sendUdsPacket(sock, targetAddr, 0x34, p);
    uint8_t res[64];
    int len = recv_with_retry(sock, res, sizeof(res));
    return checkUdsResponse(res, len, 0x74);
}

int exitTransfer(int sock, uint16_t targetAddr) {
    std::cout << "[UDS] Transfer Exit (0x37)..." << std::endl;
    sendUdsPacket(sock, targetAddr, 0x37, {});
    uint8_t res[64];
    int len = recv_with_retry(sock, res, sizeof(res));
    return checkUdsResponse(res, len, 0x77);
}

int verifyIntegrity(int sock, uint16_t targetAddr, uint32_t checksum) {
    std::cout << "[UDS] Verifying Integrity (0x31) Checksum: 0x" << std::hex << checksum << std::dec << std::endl;
    std::vector<uint8_t> p = { 0x01, 0xFF, 0x01, (uint8_t)(checksum >> 24), (uint8_t)(checksum >> 16), (uint8_t)(checksum >> 8), (uint8_t)checksum };
    sendUdsPacket(sock, targetAddr, 0x31, p);
    uint8_t res[64];
    int len = recv_with_retry(sock, res, sizeof(res));
    return checkUdsResponse(res, len, 0x71);
}

int requestBankSwap(int sock, uint16_t targetAddr) {
    std::cout << "\n[UDS] Target Activation Phase. Sending A/B Bank Swap (0x31 01 FF 02)..." << std::endl;
    std::vector<uint8_t> p = { 0x01, 0xFF, 0x02 }; 
    sendUdsPacket(sock, targetAddr, 0x31, p);
    
    uint8_t res[64];
    int len = recv_with_retry(sock, res, sizeof(res));
    return checkUdsResponse(res, len, 0x71);
}

// 단계 8 사양에 맞춤 하드 리셋(SID 0x11, Sub-function 0x01) 함수 추가
int requestEcuReset(int sock, uint16_t targetAddr) {
    std::cout << "[UDS] Sending ECU Hard Reset Command (0x11 01)..." << std::endl;
    std::vector<uint8_t> p = { 0x01 }; // 0x01: hardReset
    sendUdsPacket(sock, targetAddr, 0x11, p);

    uint8_t res[64];
    int len = recv_with_retry(sock, res, sizeof(res));
    return checkUdsResponse(res, len, 0x51); // 0x51: Positive SID (0x11 + 0x40)
}

int requestRollback(int sock, uint16_t targetAddr) {
    std::cout << "\n[UDS] Emergency Rollback Phase. Sending Rollback Reservation (0x31 01 FF 03)..." << std::endl;
    
    // Routine Control (0x31) 페이로드 구성: [startRoutine(0x01)] + [RoutineID High(0xFF)] + [RoutineID Low(0x03)]
    std::vector<uint8_t> p = { 0x01, 0xFF, 0x03 }; 
    sendUdsPacket(sock, targetAddr, 0x31, p);
    
    uint8_t res[64];
    int len = recv_with_retry(sock, res, sizeof(res));
    
    // 정상 Positive Response SID는 0x31 + 0x40 = 0x71 번으로 들어와야 합니다.
    return checkUdsResponse(res, len, 0x71); 
}

// ota_comm.cpp 사양에 맞춰 억지 HEX 라인 파싱을 걷어내고, .bin 순수 바이너리 스트리밍 방식으로 완벽 통합
int startOtaTransfer(const std::string& targetAddrStr, const std::string& version, const std::string& gatewayIp) {
    uint16_t targetAddr = (uint16_t)std::stoul(targetAddrStr, nullptr, 16);
    
    // ota_comm.cpp에서 다운로드 완료하여 보관 중인 .bin 패키지 경로 매칭
    std::string binPath = targetAddrStr + "_" + version + ".bin";
    
    std::cout << "\n[ENGINE] Loading BIN file in binary mode: " << binPath << std::endl;
    std::ifstream file(binPath, std::ios::binary | std::ios::ate); 
    if (!file.is_open()) { std::cerr << "[ERROR] Cannot open BIN file." << std::endl; return -1; }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> binaryData(size);
    if (!file.read(reinterpret_cast<char*>(binaryData.data()), size)) {
        std::cerr << "[ERROR] Failed to read binary data from file." << std::endl;
        file.close();
        return -1;
    }
    file.close();

    std::cout << "[ENGINE] Successfully loaded " << binaryData.size() << " bytes of pure binary code.\n";

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct timeval tv; tv.tv_sec = 3; tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(DOIP_PORT);
    inet_pton(AF_INET, gatewayIp.c_str(), &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "[DOIP] TCP Connection Failed!" << std::endl;
        return -1;
    }
    if (!routingActivation(sock)) {
        std::cerr << "[DOIP] Routing Activation Failed!" << std::endl;
        close(sock); return -1;
    }

    std::cout << "\n[INSTALL Phase] Executing Flashing via UDS..." << std::endl;
    int nrc;

    if ((nrc = changeDiagnosticSession(sock, targetAddr, 0x03)) != 0) {
        std::cerr << "[ERROR] Failed to enter Extended Session (0x03)" << std::endl;
        close(sock); return nrc;
    }

    // ① 0x34 Request Download (순수 단일 블록으로 단 1회 전체 크기 요청 인터락 구축)
    nrc = requestDownload(sock, targetAddr, FLASH_START_ADDRESS, binaryData.size());
    if (nrc != 0) { close(sock); return nrc; }

    // ② 0x36 Transfer Data 블록 반복 전송
    uint8_t sn = 1;
    uint32_t offset = 0;
    while (offset < binaryData.size()) {
        uint32_t currLen = (binaryData.size() - offset > 1024) ? 1024 : (binaryData.size() - offset);
        std::vector<uint8_t> udsPayload = { sn };
        udsPayload.insert(udsPayload.end(), binaryData.begin() + offset, binaryData.begin() + offset + currLen);
        
        std::cout << "[UDS] Sending 0x36 block sn: 0x" << std::hex << (int)sn 
                  << " | Offset: " << std::dec << offset << "/" << binaryData.size() << std::endl;
        current_install_progress = (offset * 100) / binaryData.size();
        sendUdsPacket(sock, targetAddr, 0x36, udsPayload);
        
        uint8_t res_buf[1500];
        int rLen = recv_with_retry(sock, res_buf, sizeof(res_buf));
        if (rLen < 0) {
            printf("recv failed: errno=%d (%s)\n", errno, strerror(errno));
            close(sock); return -1;
        }
        int transferRes = checkUdsResponse(res_buf, rLen, 0x76);
        
        if (transferRes == 0) {
            offset += currLen;
            sn = (sn == 0xFF) ? 0x00 : sn + 1; 
        } else {
            std::cerr << "[ERROR] Transfer Data failed with NRC: 0x" << std::hex << transferRes << std::dec << std::endl;
            close(sock); return transferRes;
        }
    }

    // ③ 0x37 Transfer Exit
    if ((nrc = exitTransfer(sock, targetAddr)) != 0) { close(sock); return nrc; }
    
    // ④ 0x31 Verify Integrity
    uint32_t checksum = calculateChunkCRC32(binaryData);
    if ((nrc = verifyIntegrity(sock, targetAddr, checksum)) != 0) { close(sock); return nrc; }

    std::cout << "\n✅ Flashing completed successfully!" << std::endl;
    current_state = WAIT_ACTIVATION; 
    
    bool safeStateAchieved = false;
    int retryCounter = 0;
    const int MAX_RETRIES = 120; 

    std::cout << "\n[WAIT] Vehicle data verified. Monitoring vehicle for Safe State (Stop & Gear P)..." << std::endl;

    while (!safeStateAchieved && retryCounter < MAX_RETRIES) {
        nrc = changeDiagnosticSession(sock, targetAddr, 0x02);

        if (nrc == 0) {
            std::cout << "✅ [UDS] Safe State Confirmed by ECU. Programming Session (0x10 02) Opened!" << std::endl;
            safeStateAchieved = true;
        } 
        else if (nrc == 0x22) {
            std::cout << "⚠️ [UDS] ECU Response: Conditions Not Correct (0x22). Vehicle is moving. Retrying in 3 seconds... [" 
                      << retryCounter + 1 << "/" << MAX_RETRIES << "]" << std::endl;
            
            retryCounter++;
            std::this_thread::sleep_for(std::chrono::seconds(3)); 
        } 
        else {
            std::cerr << "❌ [CRITICAL] Unexpected UDS Session Error: 0x" << std::hex << nrc << std::dec << std::endl;
            close(sock); return nrc;
        }
    }

    if (!safeStateAchieved) {
        std::cerr << "❌ [TIMEOUT] Vehicle did not enter safe state within timeout. Postponing activation." << std::endl;
        close(sock); 
        return 0x22; 
    }

    current_state = ACTIVATION; 
    int swapResult = requestBankSwap(sock, targetAddr);

    if (swapResult != 0) {
        std::cerr << "⚠️ [RECOVERY] Critical error during bank swap. Waiting for user decision..." << std::endl;
        
        // 사용자가 LCD 상에서 스위치(버튼 인터럽트)로 복구할지 선택할 수 있도록 유도
        current_state = RECOVERY; 

        // 사용자가 결정을 내릴 때까지 무한 루프 블로킹 (UI 스레드와 동기화 대기)
        while (current_state == RECOVERY) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 사용자가 ENTER 버튼 인터럽트 레이어에서 'isRecoveryGo = true'로 설정하고 넘어왔을 경우
        if (isRecoveryGo) {
            std::cout << "♻️ [ROLLBACK START] User agreed. Executing UDS Rollback Sequence..." << std::endl;

            // 1단계: RoutineControl Rollback 명령어 전송 (0x31 01 FF 03)
            int rollbackNrc = requestRollback(sock, targetAddr);
            if (rollbackNrc != 0) {
                std::cerr << "❌ [CRITICAL] ECU rejected Rollback Command! NRC: 0x" 
                          << std::hex << rollbackNrc << std::dec << std::endl;
            } else {
                std::cout << "✅ [UDS] Rollback configuration registered in target ECU." << std::endl;
            }

            // 2단계: 롤백 스위칭 유효화 적용을 위한 하드 리셋(0x11 01) 연쇄 방출
            int resetNrc = requestEcuReset(sock, targetAddr);
            if (resetNrc != 0) {
                std::cerr << "⚠️ [WARNING] ECU Reset after rollback failed. NRC: 0x" 
                          << std::hex << resetNrc << std::dec << std::endl;
            } else {
                std::cout << "🎉 [COMPLETED] Target ECU received Hard Reset and is rebooting to stable bank!" << std::endl;
            }
        } 
        else {
            // 사용자가 롤백을 취소(아니오)하고 불완전 펌웨어 영역에 그대로 놔두기를 선택했을 때
            std::cout << "⚠️ [ROLLBACK CANCELED] User declined rollback. Leaving target as-is." << std::endl;
        }

        // 롤백 처리가 끝났거나 거부되었으므로 최종 결과를 백엔드 서버에 알리기 위해 REPORTING 상태로 밀어내며 종료
        current_state = REPORTING; 
        close(sock); 
        return swapResult;
    }

    std::cout << "🚀 [SUCCESS] Bank swap command accepted. Target ECU is rebooting with new firmware..." << std::endl;
    // 뱅크 스왑 성공 직후 타겟 제어기에 단계 8 사양의 Hard Reset(0x11 01) 명령어 연쇄 방출
    int resetResult = requestEcuReset(sock, targetAddr);
    if (resetResult != 0) {
        std::cerr << "⚠️ [WARNING] Bank swap succeeded, but ECU Reset command was rejected. Code: 0x" 
                  << std::hex << resetResult << std::dec << std::endl;
    } else {
        std::cout << "🎉 [COMPLETED] Target ECU received Hard Reset and is rebooting with new firmware!" << std::endl;
    }

    // =========================================================================
    // ♻️ [롤백 스위치 대기 및 사출 선택 국면]
    // =========================================================================
    std::cout << "\n🎛️ [DEMO CHECK] 최종 롤백 시연을 위한 사용자 스위치 대기 모드로 진입합니다." << std::endl;
    
    current_state = RECOVERY; // LCD 화면을 복구 질의 창으로 임시 격하 전이
    isRecoveryGo = false;     // 플래그 초기화

    // 💡 버튼 인터럽트가 current_state를 REPORTING으로 바꿀 때까지 300ms usleep 동기화 대기
    // (이제 UI가 가비지 상태로 얼어붙거나 스위치가 안 먹는 먹통 현상이 완벽히 해결됩니다)
    while (current_state == RECOVERY) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 대기 루프가 풀린 시점(current_state == REPORTING)에서 유저의 최종 버튼 판별
    if (isRecoveryGo == true) {
        std::cout << "♻️ [ROLLBACK START] 유저가 오리지널 파티션 복구(0x31 01 FF 03)를 확정했습니다!" << std::endl;

        if ((nrc = changeDiagnosticSession(sock, targetAddr, 0x03)) != 0) {
        std::cerr << "[ERROR] Failed to enter Extended Session (0x03)" << std::endl;
        close(sock); return nrc;
        }
        
        // 1단계: RoutineControl Rollback 명령어 전송 (0x31 01 FF 03)
        int rollbackNrc = requestRollback(sock, targetAddr);
        if (rollbackNrc != 0) {
            std::cerr << "❌ [UDS ERROR] 제어기가 롤백 명령을 거부했습니다. NRC: 0x" << std::hex << rollbackNrc << std::dec << std::endl;
        } else {
            std::cout << "✅ [UDS] 타겟 제어기 롤백 예약 마킹 안착 성공." << std::endl;
        }

        // 2단계: 롤백 스위칭 유효화 적용을 위한 하드 리셋(0x11 01) 강제 트리거
        requestEcuReset(sock, targetAddr);
        std::cout << "🎉 [COMPLETED] 복구 하드 리셋 완료. 안정 원본 뱅크로 재부팅합니다." << std::endl;
    } 
    else {
        std::cout << "▶️ [COMMIT EXP] 유저가 롤백을 취소하고 신규 펌웨어 가동을 최종 락인했습니다." << std::endl;
    }

    // 💡 모든 하드웨어 통신과 스위치 판단 루틴이 끝났으므로 최종 상태 REPORTING 유지 상태로 클린업 이탈
    current_state = REPORTING; 
    close(sock);
    return 0;
}
