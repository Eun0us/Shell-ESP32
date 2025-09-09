#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "errno.h"
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SSID CONFIG_SSID

#define MAX_PROXY_RETRY 10
#define RETRY_DELAY_MS 5000

const char *TAG_PROXY = "PROXY";

void unescape_payload(const char *src, char *dest, size_t max_len) {
    size_t i = 0, j = 0;
    while (src[i] != '\0' && j < max_len - 1) {
        if (src[i] == '\\' && src[i+1] == 'r') {
            dest[j++] = '\r';
            i += 2;
        } else if (src[i] == '\\' && src[i+1] == 'n') {
            dest[j++] = '\n';
            i += 2;
        } else {
            dest[j++] = src[i++];
        }
    }
    dest[j] = '\0';
}

static struct {
    struct arg_str *host;
    struct arg_str *payload;
    struct arg_int *port;
    struct arg_end *end;
} pxy_args;

static int request(int argc, char **argv){

    int nerrors = arg_parse(argc, argv, (void **)&pxy_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, pxy_args.end, argv[0]);
        return 1;
    }
    if (pxy_args.host->count > 0 || pxy_args.payload->count > 0 || pxy_args.host->count > 0) {
        ESP_LOGE(TAG_PROXY, "ERR: arg fail");
        return 1;  
    }

    int len;
    struct sockaddr_in dest_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(pxy_args.port->ival[0]),
        .sin_addr.s_addr = inet_addr(pxy_args.host->sval[0]),
    };

    int dest_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (dest_sock < 0) {
        printf("ERR: socket fail\n");
        return 1;
    }

    if (connect(dest_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGE(TAG_PROXY, "Connexion échouée %d", dest_sock);
        printf("ERR: connect fail\n");
        close(dest_sock);
        return 1;
    }

    struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(dest_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    char real_payload[1024];
    unescape_payload(pxy_args.payload->sval[0], real_payload, sizeof(real_payload));
    int payload_len = strlen(real_payload);

    ESP_LOGI(TAG_PROXY, "Sending %d bytes...", payload_len);
    int sent = send(dest_sock, real_payload, payload_len, 0);
    if (sent < 0) {
        ESP_LOGW(TAG_PROXY, "Erreur d'envoi payload, errno=%d (%s)", errno, strerror(errno));
    }
    char rx_buf[1024];
    int total_received = 0;

    while ((len = recv(dest_sock, rx_buf, sizeof(rx_buf) - 1, 0)) > 0) {
        rx_buf[len] = 0;
        total_received += len;
        ESP_LOGI(TAG_PROXY, "%s", rx_buf);
    }

    if (total_received == 0) {
        printf("NO RESPONSE\n");
    }

    if (len < 0) {
        ESP_LOGW(TAG_PROXY, "recv() erreur, errno=%d (%s)", errno, strerror(errno));
    }

    close(dest_sock);
    ESP_LOGW(TAG_PROXY, "END OF REQUEST");
    return 0;
}

void module_proxy(void)
{
    pxy_args.host = arg_str0(NULL, NULL, "<host>", "Host address");
    pxy_args.port = arg_int0(NULL, NULL, "<port>", "Target Port");
    pxy_args.payload = arg_str0(NULL, NULL, "<payload>", "Payload");
    pxy_args.end = arg_end(1);
    const esp_console_cmd_t proxy_cmd = {
        .command = "proxy",
        .help = "send tcp/ip payload",
        .hint = NULL,
        .func = &request,
        .argtable = &pxy_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&proxy_cmd));
}