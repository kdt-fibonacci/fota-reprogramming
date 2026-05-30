#include "logger.h"

#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

/* ---------------- 로그 출력 ---------------- */
void print_log(int client_sock, const char* request) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getpeername(client_sock, (struct sockaddr*)&addr, &len);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));

    int port = ntohs(addr.sin_port);

    time_t now = time(NULL);
    struct tm* t = localtime(&now);

    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    printf("\n==================================================\n");
    printf("[%s]\n", time_str);
    printf("CLIENT: %s:%d\n\n", ip, port);
    printf("REQUEST:\n%s\n", request);
    printf("==================================================\n");
}