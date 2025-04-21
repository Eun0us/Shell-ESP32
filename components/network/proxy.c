#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "errno.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_console.h"
#include "argtable3/argtable3.h"

#include "proxy.h"

#define MAX_PROXY_RETRY 10
#define RETRY_DELAY_MS 5000

const char *TAG_INIT_PROXY = "INIT_PROXY";
const char *TAG_CMD_PROXY = "PROXY_COMMAND";
int proxy_running = 0;
int cc_client = -1;

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

void handle_proxy_command(void *pvParameter) {
    char cmd[256];
    int len;

    while (proxy_running) {
        len = recv(cc_client, cmd, sizeof(cmd) - 1, 0);
        if (len <= 0) {
            ESP_LOGE(TAG_CMD_PROXY, "Deconnected Or Error on recv()");
            break;
        }

        cmd[len] = 0;
        ESP_LOGI(TAG_CMD_PROXY, "PROXY: %s", cmd);

        char *sep1 = strchr(cmd, ':');
        char *sep2 = strchr(cmd, '|');

        if (!sep1 || !sep2 || sep2 <= sep1) {
            ESP_LOGE(TAG_CMD_PROXY, "Format invalide (attendu: ip:port|payload)");
            continue;
        }

        char ip[64] = {0};
        strncpy(ip, cmd, sep1 - cmd);
        int port = atoi(sep1 + 1);
        const char *payload = sep2 + 1;
        //int payload_len = strlen(payload);

        ESP_LOGW(TAG_CMD_PROXY, "PROXYING  --> %s:%d", ip, port);

        struct sockaddr_in dest_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr.s_addr = inet_addr(ip),
        };

        int dest_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (dest_sock < 0) {
            ESP_LOGE(TAG_CMD_PROXY, "Erreur création socket");
            continue;
        }

        if (connect(dest_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
            ESP_LOGE(TAG_CMD_PROXY, "Connexion échouée %d", dest_sock);
            close(dest_sock);
            continue;
        }
        char real_payload[1024];
        unescape_payload(payload, real_payload, sizeof(real_payload));
        int payload_len = strlen(real_payload);

        
        ESP_LOGW(TAG_CMD_PROXY, "Sending %d bytes...", payload_len);
        int sent = send(dest_sock, real_payload, payload_len, 0);
        if (sent < 0) {
            ESP_LOGE(TAG_CMD_PROXY, "Erreur d'envoi payload, errno=%d (%s)", errno, strerror(errno));
        }

        char rx_buf[1024];
        //ESP_LOGI(TAG_CMD_PROXY, "");

        while ((len = recv(dest_sock, rx_buf, sizeof(rx_buf) - 1, 0)) > 0) {
            rx_buf[len] = 0;
            send(cc_client, rx_buf, len, 0);              // Envoi au serveur
            ESP_LOGI(TAG_CMD_PROXY, "RESPONSE SEND");          // Log côté ESP32
        }
        if (len < 0) {
            ESP_LOGE(TAG_CMD_PROXY, "recv() erreur, errno=%d (%s)", errno, strerror(errno));
        }

        close(dest_sock);
        ESP_LOGW(TAG_CMD_PROXY, "END OF REQUEST");
    }

    close(cc_client);
    cc_client = -1;
    proxy_running = 0;
    ESP_LOGI(TAG_INIT_PROXY, "Proxy Close.");
    vTaskDelete(NULL);
}

static struct {
    struct arg_str *host;
    struct arg_int *port;
    struct arg_end *end;
} proxy_args;


static int init_proxy(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void **)&proxy_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, proxy_args.end, argv[0]);
        return 1;
    }

    int srv_port = proxy_args.port->ival[0];
    const char *srv_ip = proxy_args.host->sval[0];
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(srv_port),
        .sin_addr.s_addr = inet_addr(srv_ip),
    };

    int retry = 0;

    while (retry < MAX_PROXY_RETRY) {
        cc_client = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (cc_client < 0) {
            ESP_LOGE(TAG_INIT_PROXY, "Erreur socket (tentative %d)", retry + 1);
            retry++;
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            continue;
        }

        ESP_LOGI(TAG_INIT_PROXY, "Connexion à %s:%d (essai %d)...", srv_ip, srv_port, retry + 1);
        if (connect(cc_client, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
            ESP_LOGI(TAG_INIT_PROXY, "Connecté au serveur !");
            proxy_running = 1;

            xTaskCreate(&handle_proxy_command, "Handle_Command_from_srv", 8192, NULL, 1, NULL);
            return 0;
        }
        ESP_LOGE(TAG_INIT_PROXY, "Échec de connexion (essai %d)", retry + 1);
        close(cc_client);
        retry++;
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }

    ESP_LOGE(TAG_INIT_PROXY, "Impossible de se connecter après %d tentatives", MAX_PROXY_RETRY);
    return 1;
}


void module_proxy_start(void)
{
    proxy_args.host = arg_str1(NULL, NULL, "<host>", "Host address to manage proxy ");
    proxy_args.port = arg_int1(NULL, NULL, "<port>", "Port associated ");
    proxy_args.end = arg_end(1);
    const esp_console_cmd_t ping_cmd = {
        .command = "proxy_start",
        .help = "Start Proxy OPEN a tcp server",
        .hint = NULL,
        .func = &init_proxy,
        .argtable = &proxy_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ping_cmd));
}

static int proxy_stop(int argc, char **argv){
    proxy_running = 0;
    return 0;
}

void module_proxy_stop(void){
    const esp_console_cmd_t ping_cmd = {
        .command = "proxy_stop",
        .help = "Deconnect Proxy from u r server",
        .hint = NULL,
        .func = &proxy_stop
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ping_cmd));
}