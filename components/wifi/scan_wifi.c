#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "argtable3/argtable3.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "regex.h"
#include "cmd_wifi.h"

#define DEFAULT_SCAN_LIST_SIZE 20
#define USE_CHANNEL_BITMAP 1
#define CHANNEL_LIST_SIZE 3
#define NB_SCAN_DEFAULT 20
static const char *TAG = "scan";

static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_OWE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OWE");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA3_ENT_192:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENT_192");
        break;
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

/* Initialize Wi-Fi as sta and set scan method */
static int scan_wifi(int argc, char **argv)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    uint16_t ap_count = 0;

    // Allocation dynamique du tableau pour les points d'accès
    wifi_ap_record_t *ap_info = malloc(number * sizeof(wifi_ap_record_t));
    if (ap_info == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }

    memset(ap_info, 0, number * sizeof(wifi_ap_record_t));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Scanning Wi-Fi networks...");
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true)); // Démarre le scan en mode blocage

    // Attente explicite pour permettre au scan de se terminer (5 secondes ici)
    vTaskDelay(pdMS_TO_TICKS(5000)); 

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    if (ap_count > number) {
        ESP_LOGW(TAG, "Too many APs, increasing buffer size");
        number = ap_count;  // Ajuste la taille du tableau
        ap_info = realloc(ap_info, number * sizeof(wifi_ap_record_t)); // Réalloue la mémoire si nécessaire
        if (ap_info == NULL) {
            ESP_LOGE(TAG, "Memory reallocation failed");
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));

    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);

    for (int i = 0; i < number; i++) {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI(TAG, "BSSID: %02X:%02X:%02X:%02X:%02X:%02X",
                 ap_info[i].bssid[0], ap_info[i].bssid[1], ap_info[i].bssid[2],
                 ap_info[i].bssid[3], ap_info[i].bssid[4], ap_info[i].bssid[5]);
        print_auth_mode(ap_info[i].authmode);
        ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
    }

    free(ap_info); // Libère la mémoire après l'utilisation

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    esp_netif_destroy(sta_netif);
    ESP_ERROR_CHECK(esp_event_loop_delete_default());

    return 0;
}

void module_scan_wifi(void)
{
    const esp_console_cmd_t scan_cmd = {
        .command = "scan-wifi",
        .help = "scan all network around us",
        .hint = NULL,
        .func = &scan_wifi
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&scan_cmd) );
}
