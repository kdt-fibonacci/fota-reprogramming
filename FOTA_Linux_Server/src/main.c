#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"

char SERVER_IP[64];
int PORT;

/* ========================= */
/* ENV LOAD */
/* ========================= */

void load_env()
{
    FILE* fp =
        fopen("includes/.env", "r");

    if (!fp)
    {
        printf(
            "Failed to open .env\n"
        );

        exit(1);
    }

    /* ------------------------- */
    /* SERVER IP */
    /* ------------------------- */

    fgets(
        SERVER_IP,
        sizeof(SERVER_IP),
        fp
    );

    SERVER_IP[
        strcspn(
            SERVER_IP,
            "\r\n"
        )
    ] = 0;

    /* ------------------------- */
    /* PORT */
    /* ------------------------- */

    char port_str[32];

    fgets(
        port_str,
        sizeof(port_str),
        fp
    );

    PORT = atoi(port_str);

    fclose(fp);
}

/* ========================= */
/* MAIN */
/* ========================= */

int main()
{
    load_env();

    printf(
        "SERVER_IP : %s\n",
        SERVER_IP
    );

    printf(
        "PORT      : %d\n",
        PORT
    );

    start_server();

    return 0;
}