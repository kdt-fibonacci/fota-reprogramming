#include "file.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>

/* file send */
void send_file(int client_sock, const char* filename) {
    int fd = open(filename, O_RDONLY);

    if (fd < 0) {
        const char* not_found =
            "HTTP/1.1 404 Not Found\r\n\r\nFile not found";

        send(client_sock, not_found, strlen(not_found), 0);
        return;
    }

    const char* header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n\r\n";

    send(client_sock, header, strlen(header), 0);

    char buffer[BUFFER_SIZE];
    int bytes;

    while ((bytes = read(fd, buffer, BUFFER_SIZE)) > 0) {
        send(client_sock, buffer, bytes, 0);
    }

    close(fd);
}


/* file size */
long get_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0)
        return st.st_size;
    return -1;
}