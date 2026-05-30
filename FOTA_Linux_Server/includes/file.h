#ifndef FILE_H
#define FILE_H

void send_file(
    int client_sock,
    const char* filename
);

long get_file_size(
    const char* path
);

#endif