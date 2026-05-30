#include "mqtt_handler.h"

#include <stdio.h>
#include <string.h>

#include "MQTTClient.h"

#include "crypto.h"

extern char SERVER_IP[64];
extern int PORT;

/* ========================= */
/* MQTT OTA PUBLISH */
/* ========================= */

void publish_update_notification(
    const char* address,
    const char* version
)
{
    MQTTClient client;

    MQTTClient_connectOptions conn_opts =
        MQTTClient_connectOptions_initializer;

    MQTTClient_create(
        &client,
        MQTT_ADDRESS,
        MQTT_CLIENT_ID,
        MQTTCLIENT_PERSISTENCE_NONE,
        NULL
    );

    int rc;

    rc = MQTTClient_connect(
        client,
        &conn_opts
    );

    if (rc != MQTTCLIENT_SUCCESS)
    {
        printf(
            "[MQTT] connect failed : %d\n",
            rc
        );

        return;
    }

    /* ========================= */
    /* HEX PATH */
    /* ========================= */

    char hex_path[256];

    sprintf(
        hex_path,
        "hex/%s/%s.hex",
        address,
        version
    );

    /* ========================= */
    /* SHA256 */
    /* ========================= */

    char checksum[65];

    calculate_sha256(
        hex_path,
        checksum
    );

    /* ========================= */
    /* JSON PAYLOAD */
    /* ========================= */

    char payload[2048];

    sprintf(
        payload,

        "{"

        "\"address\":\"%s\","
        "\"version\":\"%s\","

        "\"firmware_url\":"
        "\"http://%s:%d/ota/down/hex/%s/%s.hex\","

        "\"signature_url\":"
        "\"http://%s:%d/ota/down/sig/%s/%s.sig\","

        "\"checksum\":\"%s\""

        "}",

        address,
        version,

        SERVER_IP,
        PORT,
        address,
        version,

        SERVER_IP,
        PORT,
        address,
        version,

        checksum
    );

    /* ========================= */
    /* MQTT MESSAGE */
    /* ========================= */

    MQTTClient_message pubmsg =
        MQTTClient_message_initializer;

    pubmsg.payload = payload;

    pubmsg.payloadlen =
        strlen(payload);

    pubmsg.qos = MQTT_QOS;

    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;

    MQTTClient_publishMessage(
        client,
        MQTT_TOPIC,
        &pubmsg,
        &token
    );

    MQTTClient_waitForCompletion(
        client,
        token,
        MQTT_TIMEOUT
    );

    /* ========================= */
    /* LOG */
    /* ========================= */

    printf("\n");

    printf(
        "====================================\n"
    );

    printf(
        "[MQTT OTA NOTIFICATION]\n"
    );

    printf(
        "ECU ADDRESS : %s\n",
        address
    );

    printf(
        "VERSION     : %s\n",
        version
    );

    printf(
        "====================================\n"
    );

    printf(
        "TOPIC : %s\n",
        MQTT_TOPIC
    );

    printf(
        "PAYLOAD:\n%s\n",
        payload
    );

    /* ========================= */
    /* CLEANUP */
    /* ========================= */

    MQTTClient_disconnect(
        client,
        1000
    );

    MQTTClient_destroy(
        &client
    );
}