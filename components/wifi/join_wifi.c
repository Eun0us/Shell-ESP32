#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"

#define JOIN_TIMEOUT_MS (10000)
#define TAG "join_wifi"

static EventGroupHandle_t wifi_event_group = NULL;
static bool wifi_stack_initialized = false;
const int CONNECTED_BIT = BIT0;

// Handler WiFi STA
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

// Toutes les inits système, appelle UNE SEULE FOIS
static void ensure_wifi_stack_init(void)
{
    if (wifi_stack_initialized)
        return;

    // Init ESP stack au besoin
    static bool netif_inited = false;
    if (!netif_inited) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();
        netif_inited = true;
    }

    wifi_event_group = xEventGroupCreate();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    wifi_stack_initialized = true;
}

// Fonction de connexion WiFi (réutilisable)
static bool wifi_join(const char *ssid, const char *pass, int timeout_ms)
{
    ensure_wifi_stack_init();

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass) {
        strlcpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }

    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_connect() );

    int bits = xEventGroupWaitBits(
        wifi_event_group, CONNECTED_BIT,
        pdFALSE, pdTRUE,
        timeout_ms / portTICK_PERIOD_MS
    );
    return (bits & CONNECTED_BIT) != 0;
}

// Argument parsing struct
static struct {
    struct arg_int *timeout;
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} join_args;

// Command handler
static int join_cmd_func(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&join_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, join_args.end, argv[0]);
        return 1;
    }

    int timeout = JOIN_TIMEOUT_MS;
    if (join_args.timeout->count > 0)
        timeout = join_args.timeout->ival[0];

    const char *ssid = join_args.ssid->sval[0];
    const char *pass = join_args.password->count > 0 ? join_args.password->sval[0] : NULL;

    ESP_LOGI(TAG, "Connecting to SSID: '%s'", ssid);

    bool connected = wifi_join(ssid, pass, timeout);
    if (!connected) {
        ESP_LOGW(TAG, "Connection timed out or failed");
        return 1;
    }
    ESP_LOGI(TAG, "Connected successfully");
    return 0;
}

// Fonction d'enregistrement de la commande (appelle juste dans app_main)
void register_join_wifi_cmd(void)
{
    join_args.timeout  = arg_int0(NULL, "timeout", "<ms>", "Connection timeout (ms)");
    join_args.ssid     = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    join_args.password = arg_str0(NULL, NULL, "<pass>", "Password (optional)");
    join_args.end      = arg_end(2);

    const esp_console_cmd_t join_cmd = {
        .command = "join",
        .help = "Join WiFi AP as a station",
        .hint = NULL,
        .func = &join_cmd_func,
        .argtable = &join_args,
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&join_cmd) );
}
