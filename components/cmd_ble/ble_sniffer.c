#include <stdio.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

// Log tag
static const char *TAG = "BLE_Sniffer";

// Callback pour les événements BLE
void gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            esp_gap_search_evt_t search_evt = param->scan_rst.search_evt;

            // Vérifier si c'est un rapport publicitaire
            if (search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                // Adresse MAC de l'appareil
                printf("Adresse MAC : %02x:%02x:%02x:%02x:%02x:%02x\n",
                       param->scan_rst.bda[0], param->scan_rst.bda[1], param->scan_rst.bda[2],
                       param->scan_rst.bda[3], param->scan_rst.bda[4], param->scan_rst.bda[5]);

                // RSSI (force du signal)
                printf("RSSI : %d dBm\n", param->scan_rst.rssi);

                // Données de publicité
                printf("Données de publicité : ");
                for (int i = 0; i < param->scan_rst.adv_data_len; i++) {
                    printf("%02x ", param->scan_rst.ble_adv[i]);
                }
                printf("\n");

                // Données du scan réponse
                if (param->scan_rst.scan_rsp_len > 0) {
                    printf("Données du scan réponse : ");
                    for (int i = 0; i < param->scan_rst.scan_rsp_len; i++) {
                        printf("%02x ", param->scan_rst.ble_adv[param->scan_rst.adv_data_len + i]);
                    }
                    printf("\n");
                }
            }
            break;
        }
        default:
            ESP_LOGI(TAG, "Événement BLE non géré : %d", event);
            break;
    }
}

void ble_sniffer_start() {
    // Initialisation du contrôleur BLE
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    // Initialisation de la pile Bluedroid
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // Enregistrement du callback GAP
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_callback));

    // Démarrage du scan BLE
    esp_ble_scan_params_t scan_params = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50,
        .scan_window = 0x30
    };
    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&scan_params));

    ESP_LOGI(TAG, "Sniffer BLE démarré.");
}

void ble_sniffer_stop() {
    ESP_LOGI(TAG, "Arrêt du sniffer BLE.");
    ESP_ERROR_CHECK(esp_ble_gap_stop_scanning());
    ESP_ERROR_CHECK(esp_bluedroid_disable());
    ESP_ERROR_CHECK(esp_bluedroid_deinit());
    ESP_ERROR_CHECK(esp_bt_controller_disable());
    ESP_ERROR_CHECK(esp_bt_controller_deinit());
}

static int sniffer_ble() {
    ESP_LOGI(TAG, "Initialisation du sniffer BLE.");
    ble_sniffer_start();

    // Boucle principale pour maintenir le programme en exécution
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ble_sniffer_stop();
    return 0;
}

void register_sniffer_ble(void) {
    const esp_console_cmd_t sniff_cmd_ble = {
        .command = "sniffer_ble",
        .help = "Démarre le sniffer BLE et affiche les publicités BLE",
        .hint = NULL,
        .func = &sniffer_ble
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&sniff_cmd_ble));
}
