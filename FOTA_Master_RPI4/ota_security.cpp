// ota_security.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>

void printOpenSSLErrors() {
    ERR_print_errors_fp(stderr);
}

std::vector<unsigned char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> buffer(size);
    if (!file.read((char*)buffer.data(), size)) return {};

    return buffer;
}

bool verifyFirmwareSecurity(const std::string& addr, const std::string& version, const std::string& publicKeyPath) {
    // 💡 [변경 완료]: 검증할 타겟 파일 포맷을 .hex에서 순수 이진 파일(.bin)로 변경했습니다.
    std::string binFilename = addr + "_" + version + ".bin";
    std::string sigFilename = addr + "_" + version + ".sig";

    std::cout << ">> [Security] Verifying Pure Binary: " << binFilename << " with " << sigFilename << std::endl;

    std::vector<unsigned char> firmwareData = readFile(binFilename);
    std::vector<unsigned char> signature = readFile(sigFilename);

    if (firmwareData.empty() || signature.empty()) {
        std::cerr << ">> [Error] Failed to load binary firmware or signature file." << std::endl;
        return false;
    }

    FILE* pubKeyFile = fopen(publicKeyPath.c_str(), "r");
    if (!pubKeyFile) {
        std::cerr << ">> [Error] Cannot open public key: " << publicKeyPath << std::endl;
        return false;
    }

    EVP_PKEY* pkey = PEM_read_PUBKEY(pubKeyFile, NULL, NULL, NULL);
    fclose(pubKeyFile);

    if (!pkey) {
        std::cerr << ">> [Error] Failed to read public key." << std::endl;
        printOpenSSLErrors();
        return false;
    }

    EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
    bool isSuccess = false;

    if (EVP_DigestVerifyInit(mdCtx, NULL, EVP_sha256(), NULL, pkey) <= 0) {
        std::cerr << ">> [Error] DigestVerifyInit failed." << std::endl;
    } else {
        if (EVP_DigestVerifyUpdate(mdCtx, firmwareData.data(), firmwareData.size()) <= 0) {
            std::cerr << ">> [Error] DigestVerifyUpdate failed." << std::endl;
        } else {
            int result = EVP_DigestVerifyFinal(mdCtx, signature.data(), signature.size());
            if (result == 1) {
                std::cout << "✅ [Success] ECDSA Verification Passed for " << binFilename << "!" << std::endl;
                isSuccess = true;
            } else if (result == 0) {
                std::cerr << "❌ [Critical] Signature Mismatch! Binary file may be corrupted or tampered." << std::endl;
            } else {
                std::cerr << ">> [Error] Error during verification final." << std::endl;
                printOpenSSLErrors();
            }
        }
    }

    EVP_MD_CTX_free(mdCtx);
    EVP_PKEY_free(pkey);

    return isSuccess;
}
