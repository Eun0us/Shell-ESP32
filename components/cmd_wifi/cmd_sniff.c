#include <stdio.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_console.h"

// Log tag
static const char *TAG = "WiFi_Sniffer";

// Structure pour afficher les types de trames
const char *frame_type(uint8_t type) {
    switch (type) {
        case 0x08: return "Data Frame";
        case 0x80: return "Beacon Frame";
        case 0x40: return "Probe Request";
        case 0x50: return "Probe Response";
        case 0x00: return "Management Frame";
        default: return "Unknown Frame";
    }
}

// Fonction pour extraire le SSID d'une trame Beacon
void extract_ssid(const uint8_t *payload, uint16_t length) {
    if (length <= 36) {
        ESP_LOGW(TAG, "Trame trop courte pour contenir un SSID");
        return;
    }

    uint8_t ssid_length = payload[37]; // Longueur du SSID

    if (ssid_length > 0 && (37 + ssid_length < length)) {
        printf("SSID : ");
        for (int i = 0; i < ssid_length; i++) {
            printf("%c", payload[38 + i]);
        }
        printf("\n");
    } else {
        ESP_LOGW(TAG, "Aucun SSID ou longueur invalide");
    }
}

// Fonction pour afficher une analyse détaillée de la trame
void analyze_frame(const uint8_t *payload, uint16_t length) {
    if (length < 24) {
        ESP_LOGW(TAG, "Trame trop courte pour une analyse détaillée");
        return;
    }

    // Extraire le champ Frame Control
    uint16_t frame_control = (payload[1] << 8) | payload[0];
    uint8_t version = frame_control & 0x03;
    uint8_t type = (frame_control >> 2) & 0x03;
    uint8_t subtype = (frame_control >> 4) & 0x0F;

    printf("Version : %d, Type : %d, Sous-type : %d\n", version, type, subtype);

    // Afficher les adresses MAC
    printf("Adresse Destination : %02x:%02x:%02x:%02x:%02x:%02x\n",
           payload[4], payload[5], payload[6], payload[7], payload[8], payload[9]);
    printf("Adresse Source : %02x:%02x:%02x:%02x:%02x:%02x\n",
           payload[10], payload[11], payload[12], payload[13], payload[14], payload[15]);
    printf("Adresse BSSID : %02x:%02x:%02x:%02x:%02x:%02x\n",
           payload[16], payload[17], payload[18], payload[19], payload[20], payload[21]);

    // Analyser les champs spécifiques si Beacon
    if (subtype == 0x08) { // Beacon Frame
        uint64_t timestamp = 0;
        for (int i = 0; i < 8; i++) {
            timestamp |= ((uint64_t)payload[24 + i] << (i * 8));
        }
        uint16_t beacon_interval = (payload[32] | (payload[33] << 8));
        uint16_t capabilities = (payload[34] | (payload[35] << 8));

        printf("Timestamp : %llu\n", timestamp);
        printf("Intervalle Beacon : %d ms\n", beacon_interval);
        printf("Capabilities : 0x%04x\n", capabilities);

        // Extraire le SSID
        extract_ssid(payload, length);
    }
}

void promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *payload = pkt->payload;
    uint16_t length = pkt->rx_ctrl.sig_len;

    // Vérifier si c'est une trame de type Data
    uint8_t frame_control = payload[0];
    uint8_t frame_type = (frame_control >> 2) & 0x03;
    uint8_t frame_subtype = (frame_control >> 4) & 0x0F;

    ESP_LOGI(TAG, "Payload brut de la trame :");
    for (int i = 0; i < length && i < 64; i++) {
        printf("%02x ", payload[i]);
    }
    printf("\n");

    // Filtrer les trames Beacon pour récupérer le SSID
    if (frame_type == 0x00 && frame_subtype == 0x08) {
        ESP_LOGI(TAG, "Trame Beacon détectée - Longueur : %d", length);
        analyze_frame(payload, length);
    }

    // Filtrer les trames de données (Data Frame)
    if (frame_type == 2 && frame_subtype == 8) {
        ESP_LOGI(TAG, "Trame de données détectée (possible handshake) - Longueur : %d", length);

        // Rechercher des trames EAPOL
        if (length > 34 && payload[30] == 0x88 && payload[31] == 0x8E) {
            ESP_LOGI(TAG, "Trame EAPOL détectée (Handshake WPA/WPA2)");

            // Afficher les premiers octets de la trame
            printf("EAPOL Payload : ");
            for (int i = 0; i < length && i < 64; i++) {
                printf("%02x ", payload[i]);
            }
            printf("\n\n");
        }
    }
}

void wifi_init_promiscuous() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialisation du Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Configuration du Wi-Fi en mode station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Activation du mode promiscuous
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

    // Configuration du callback pour les trames capturées
    esp_wifi_set_promiscuous_rx_cb(promiscuous_callback);

    // Démarrage du Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Mode promiscuous activé.");
}

void stop_sniffer() {
    ESP_LOGI(TAG, "Arrêt du mode promiscuous Wi-Fi");
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
}

static int sniffer() {
    ESP_LOGI(TAG, "Initialisation du mode promiscuous Wi-Fi");
    wifi_init_promiscuous();

    // Boucle principale pour maintenir le programme en exécution
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    stop_sniffer();
    return 0;
}

void register_sniffer(void) {
    const esp_console_cmd_t sniff_cmd = {
        .command = "sniffer",
        .help = "Démarre le mode promiscuous et affiche les trames Wi-Fi",
        .hint = NULL,
        .func = &sniffer
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&sniff_cmd));
}
