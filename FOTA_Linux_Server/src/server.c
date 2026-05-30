#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <microhttpd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "ota.h"
#include "mqtt_handler.h"
#include "crypto.h"

#define POST_BUFFER_SIZE 1024

extern int PORT;

/* ========================= */
/* CONNECTION INFO            */
/* ========================= */

/* HTTP 요청 하나당 생성되는 연결 상태 구조체.
   multipart POST 파싱, HEX 파일 저장, 요청 바디 버퍼링에 사용된다. */
struct ConnectionInfo
{
    struct MHD_PostProcessor* post_processor; /* multipart 파서 핸들 */

    FILE* hex_fp;       /* 업로드 중인 HEX 파일 핸들 */

    char address[32];   /* 대상 ECU 주소 */
    char version[32];   /* 펌웨어 버전 문자열 */

    char body[8192];    /* /ota/check, /ota/report 용 raw JSON 바디 */
    size_t body_size;
};

/* ========================= */
/* MULTIPART PARSER           */
/* ========================= */

/* MHD_create_post_processor 에 등록되는 콜백.
   multipart 필드가 파싱될 때마다 호출되며
   address / version / hex_file 세 필드를 처리한다. */
static int iterate_post(
    void* coninfo_cls,
    enum MHD_ValueKind kind,
    const char* key,
    const char* filename,
    const char* content_type,
    const char* transfer_encoding,
    const char* data,
    uint64_t off,
    size_t size
)
{
    struct ConnectionInfo* con_info = (struct ConnectionInfo*)coninfo_cls;

    if (strcmp(key, "address") == 0)
    {
        /* 대상 ECU 주소 저장 */
        snprintf(con_info->address, sizeof(con_info->address), "%.*s", (int)size, data);
    }
    else if (strcmp(key, "version") == 0)
    {
        /* 펌웨어 버전 저장 */
        snprintf(con_info->version, sizeof(con_info->version), "%.*s", (int)size, data);
    }
    else if (strcmp(key, "hex_file") == 0)
    {
        /* HEX 파일을 hex/<address>/<version>.hex 경로로 스트리밍 저장.
           off == 0 이면 첫 번째 청크이므로 파일을 새로 연다. */
        char path[256];
        sprintf(path, "hex/%s/%s.hex", con_info->address, con_info->version);

        if (off == 0)
        {
            printf("[UPLOAD HEX] %s\n", path);

            con_info->hex_fp = fopen(path, "wb");
            if (!con_info->hex_fp) { perror("fopen hex"); return MHD_NO; }
        }

        if (con_info->hex_fp)
            fwrite(data, 1, size, con_info->hex_fp);
    }

    return MHD_YES;
}

/* ========================= */
/* HTTP HANDLER               */
/* ========================= */

/* MHD 메인 요청 콜백. 모든 HTTP 요청이 이 함수를 통해 처리된다.
   con_cls 가 NULL 이면 최초 호출이므로 ConnectionInfo 를 초기화한다. */
static int answer_to_connection(
    void* cls,
    struct MHD_Connection* connection,
    const char* url,
    const char* method,
    const char* version,
    const char* upload_data,
    size_t* upload_data_size,
    void** con_cls
)
{
    /* ── 최초 호출: ConnectionInfo 할당 ── */
    if (*con_cls == NULL)
    {
        struct ConnectionInfo* con_info = calloc(1, sizeof(struct ConnectionInfo));

        /* POST 요청이면 multipart 파서를 함께 생성 */
        if (strcmp(method, "POST") == 0)
        {
            con_info->post_processor =
                MHD_create_post_processor(connection, POST_BUFFER_SIZE, iterate_post, con_info);
        }

        *con_cls = con_info;
        return MHD_YES;
    }

    struct ConnectionInfo* con_info = (struct ConnectionInfo*)(*con_cls);

    /* ========================= */
    /* POST                       */
    /* ========================= */

    if (strcmp(method, "POST") == 0)
    {
        /* ── 바디 수신 중 ── */
        if (*upload_data_size != 0)
        {
            /* /ota/check, /ota/report 는 raw JSON 바디를 별도 버퍼에 누적 */
            if (strcmp(url, "/ota/check") == 0 || strcmp(url, "/ota/report") == 0)
            {
                if (con_info->body_size + *upload_data_size < sizeof(con_info->body) - 1)
                {
                    memcpy(con_info->body + con_info->body_size, upload_data, *upload_data_size);
                    con_info->body_size += *upload_data_size;
                    con_info->body[con_info->body_size] = '\0';
                }
            }

            /* multipart 파서에도 데이터 전달 */
            if (con_info->post_processor)
                MHD_post_process(con_info->post_processor, upload_data, *upload_data_size);

            *upload_data_size = 0;
            return MHD_YES;
        }

        /* ── /ota/check: ECU 가 서버에 업데이트 여부를 묻는 요청 ── */
        if (strcmp(url, "/ota/check") == 0)
        {
            printf("\n====================================\n");
            printf("[OTA CHECK REQUEST]\n%s\n", con_info->body);
            printf("====================================\n");

            /* 요청 JSON 을 파싱해 응답 JSON 생성 */
            char response_json[8192];
            build_check_response_json(con_info->body, response_json);

            struct MHD_Response* response =
                MHD_create_response_from_buffer(strlen(response_json),
                                                (void*)response_json,
                                                MHD_RESPMEM_MUST_COPY);

            int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
            MHD_destroy_response(response);
            return ret;
        }

        /* ── /ota/report: ECU 가 OTA 결과를 보고하는 요청 ── */
        if (strcmp(url, "/ota/report") == 0)
        {
            printf("\n====================================\n");
            printf("[OTA REPORT RECEIVED]\n%s\n", con_info->body);
            printf("====================================\n");

            const char* json = "{\"result\":\"ok\"}";

            struct MHD_Response* response =
                MHD_create_response_from_buffer(strlen(json),
                                                (void*)json,
                                                MHD_RESPMEM_PERSISTENT);

            int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);

            /* 수신한 리포트를 대시보드 서버로 포워딩 */
            forward_report_to_dashboard(con_info->body);

            MHD_destroy_response(response);
            return ret;
        }

        /* ── /upload: 대시보드에서 HEX 파일을 업로드하는 요청 ── */
        if (strcmp(url, "/upload") == 0)
        {
            /* 스트리밍 저장이 끝났으므로 파일 핸들 닫기 */
            if (con_info->hex_fp) { fclose(con_info->hex_fp); con_info->hex_fp = NULL; }

            printf("\n====================================\n");
            printf("[UPLOAD COMPLETE]\nADDRESS : %s\nVERSION : %s\n",
                   con_info->address, con_info->version);
            printf("====================================\n");

            /* ── 서명 파일 생성 ── */
            char hex_path[256], sig_path[256];
            sprintf(hex_path, "hex/%s/%s.hex", con_info->address, con_info->version);
            sprintf(sig_path, "sig/%s/%s.sig", con_info->address, con_info->version);

            generate_sig_file(hex_path, sig_path);
            printf("[SIG GENERATED] %s\n", sig_path);

            /* ── version.list 에 버전 추가 ──
               마지막 줄에 개행이 없으면 먼저 추가한 뒤 버전을 기록한다. */
            char version_path[256];
            sprintf(version_path, "hex/%s/version.list", con_info->address);

            FILE* fp = fopen(version_path, "a");
            if (fp)
            {
                fseek(fp, 0, SEEK_END);
                long size = ftell(fp);

                if (size > 0)
                {
                    fseek(fp, -1, SEEK_END);
                    if (fgetc(fp) != '\n') { fseek(fp, 0, SEEK_END); fprintf(fp, "\n"); }
                }

                fprintf(fp, "%s\n", con_info->version);
                fclose(fp);
            }

            /* ── MQTT 로 업데이트 알림 발행 ── */
            publish_update_notification(con_info->address, con_info->version);

            const char* json = "{\"result\":\"ok\"}";

            struct MHD_Response* response =
                MHD_create_response_from_buffer(strlen(json),
                                                (void*)json,
                                                MHD_RESPMEM_PERSISTENT);

            int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
            MHD_destroy_response(response);
            return ret;
        }
    }

    /* ========================= */
    /* GET                        */
    /* ========================= */

    if (strcmp(method, "GET") == 0)
    {
        /* ── /ota/down/<path>: ECU 가 HEX 또는 SIG 파일을 다운로드하는 요청 ── */
        if (strncmp(url, "/ota/down/", 10) == 0)
        {
            /* URL 에서 앞의 "/ota/down/" 을 제거하면 파일 경로 */
            FILE* fp = fopen(url + 10, "rb");
            if (!fp) return MHD_NO;

            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            rewind(fp);

            char* buffer = malloc(size);
            fread(buffer, 1, size, fp);
            fclose(fp);

            struct MHD_Response* response =
                MHD_create_response_from_buffer(size, buffer, MHD_RESPMEM_MUST_FREE);

            int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
            MHD_destroy_response(response);
            return ret;
        }

        /* ── /ota/key/public.pem: ECU 가 서버 공개키를 가져가는 요청 ── */
        if (strcmp(url, "/ota/key/public.pem") == 0)
        {
            FILE* fp = fopen("keys/public.pem", "rb");
            if (!fp) return MHD_NO;

            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            rewind(fp);

            char* buffer = malloc(size);
            fread(buffer, 1, size, fp);
            fclose(fp);

            struct MHD_Response* response =
                MHD_create_response_from_buffer(size, buffer, MHD_RESPMEM_MUST_FREE);

            int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
            MHD_destroy_response(response);
            return ret;
        }
    }

    return MHD_NO;
}

/* ========================= */
/* START SERVER               */
/* ========================= */

/* MHD 데몬을 시작하고 Enter 입력 전까지 블로킹.
   내부 폴링 스레드 모드를 사용하므로 별도 이벤트 루프 불필요. */
void start_server()
{
    struct MHD_Daemon* daemon =
        MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, PORT,
                         NULL, NULL,
                         answer_to_connection, NULL,
                         MHD_OPTION_END);

    if (!daemon) { printf("Server start failed\n"); return; }

    printf("OTA Server running on port %d\n", PORT);

    getchar(); /* Enter 로 서버 종료 */

    MHD_stop_daemon(daemon);
}

/* ========================= */
/* FORWARD REPORT             */
/* ========================= */

/* /ota/report 로 받은 JSON 을 로컬 대시보드(Flask, :5000)로 포워딩.
   TCP 소켓으로 직접 HTTP POST 를 보낸 뒤 즉시 닫는다. */
void forward_report_to_dashboard(const char* json_body)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return; }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(5000);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connect");
        close(sock);
        return;
    }

    /* Content-Length 를 포함한 최소한의 HTTP/1.1 요청 헤더 조립 */
    char request[4096];
    sprintf(request,
            "POST /api/log HTTP/1.1\r\n"
            "Host: 127.0.0.1:5000\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %ld\r\n"
            "\r\n"
            "%s",
            strlen(json_body), json_body);

    send(sock, request, strlen(request), 0);
    close(sock);
}