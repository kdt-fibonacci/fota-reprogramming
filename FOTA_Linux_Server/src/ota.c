#include "ota.h"
#include "config.h"
#include "crypto.h"
#include "file.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

extern char SERVER_IP[64];
extern int PORT;


/* ECU별 최신 버전 읽기 */
void get_latest_version(
    const char* ecu_address,
    char* version
) {

    char path[128];

    sprintf(
        path,
        "hex/%s/version.list",
        ecu_address
    );

    FILE* fp = fopen(path, "r");

    if (!fp) {

        strcpy(version, "0.0");

        return;
    }

    char line[64];

    while (fgets(line, sizeof(line), fp)) {

        line[strcspn(line, "\r\n")] = 0;

        if (strlen(line) > 0) {

            strcpy(version, line);
        }
    }

    fclose(fp);
}


/* 버전 비교 */
int compare_version(
    const char* v1,
    const char* v2
) {
    int a, b, c, d;

    sscanf(v1, "%d.%d", &a, &b);
    sscanf(v2, "%d.%d", &c, &d);

    if (a != c)
        return a < c;

    return b < d;
}

void build_check_response_json(
    const char* request,
    char* response
)
{
    strcpy(response,
        "{"
        "\"updates\":["
    );

    int first = 1;

    const char* ptr = request;

    while ((ptr = strstr(ptr, "\"address\"")))
    {
        char address[32];
        char version[32];

        sscanf(
            ptr,
            "\"address\": \"%31[^\"]\"",
            address
        );

        const char* vptr =
            strstr(ptr, "\"version\"");

        if (!vptr)
            break;

        sscanf(
            vptr,
            "\"version\": \"%31[^\"]\"",
            version
        );

        char latest[32];

        get_latest_version(
            address,
            latest
        );

        int need_update =
            compare_version(
                version,
                latest
            );

        if (!first)
        {
            strcat(response, ",");
        }

        first = 0;

        /* ========================= */
        /* NO UPDATE */
        /* ========================= */

        if (!need_update)
        {
            char item[512];

            sprintf(
                item,
                "{"
                "\"address\":\"%s\","
                "\"update\":false"
                "}",
                address
            );

            strcat(response, item);
        }

        /* ========================= */
        /* UPDATE AVAILABLE */
        /* ========================= */

        else
        {
            char hex_path[256];

            sprintf(
                hex_path,
                "hex/%s/%s.hex",
                address,
                latest
            );

            long size =
                get_file_size(hex_path);

            char checksum[65];

            calculate_sha256(
                hex_path,
                checksum
            );

            char item[2048];

            sprintf(
                item,

                "{"
                "\"address\":\"%s\","
                "\"update\":true,"
                "\"version\":\"%s\","

                "\"firmware_url\":"
                "\"http://%s:%d/ota/down/hex/%s/%s.hex\","

                "\"signature_url\":"
                "\"http://%s:%d/ota/down/sig/%s/%s.sig\","

                "\"checksum\":\"%s\","
                "\"size\":%ld"
                "}",

                address,
                latest,

                SERVER_IP,
                PORT,
                address,
                latest,

                SERVER_IP,
                PORT,
                address,
                latest,

                checksum,
                size
            );

            strcat(response, item);
        }

        ptr = vptr + 1;
    }

    strcat(response, "]}");
}