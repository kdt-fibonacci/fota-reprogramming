#ifndef OTA_H
#define OTA_H

int compare_version(const char* v1, const char* v2);

void get_latest_version(
    const char* address,
    char* version
);

void build_check_response_json(
    const char* request_body,
    char* response_json
);

#endif