#include "utils.h"

#include <string.h>

/* path parsing */
void extract_path(const char* request, char* path) {
    const char* start = strstr(request, "GET ");
    if (!start) return;

    start += 4;

    int i = 0;
    while (start[i] != ' ' && start[i] != '\0' && i < 127) {
        path[i] = start[i];
        i++;
    }
    path[i] = '\0';

    if (strncmp(path, "/ota/down/", 10) == 0) {
        memmove(path, path + 10, strlen(path) - 9);
    }
}

