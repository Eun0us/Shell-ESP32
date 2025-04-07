#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"

// Log tag
static const char *TAG = "WiFi_Sniffer";

// Déclaration de la fonction stop_sniffer avant son utilisation
void stop_sniffer(void);

// Fonction pour extraire le SSID des trames Beacon ou Probe Response
void extract_ssid(const uint8_t *payload, uint16_t length) {
    if (length < 36) {
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

// Fonction pour analyser une trame Beacon ou Probe Response
void analyze_beacon_or_probe(const uint8_t *payload, uint16_t length) {
    uint8_t frame_control = payload[0];
    uint8_t type = (frame_control >> 2) & 0x03;  // Type de trame (0x00 pour management, 0x01 pour contrôle, 0x02 pour données)
    uint8_t subtype = (frame_control >> 4) & 0x0F;  // Sous-type de trame (0x08 pour Beacon, 0x04 pour Probe Request, etc.)

    if (type == 0x00 && (subtype == 0x08 || subtype == 0x04 || subtype == 0x05)) { // Beacon ou Probe Request/Response
        ESP_LOGI(TAG, "Type de trame : %s, Sous-type : 0x%02x", (subtype == 0x08) ? "Beacon" : (subtype == 0x04) ? "Probe Request" : "Probe Response", subtype);

        // Extraire et afficher les informations sur les adresses MAC
        printf("Adresse Destination : %02x:%02x:%02x:%02x:%02x:%02x\n", payload[4], payload[5], payload[6], payload[7], payload[8], payload[9]);
        printf("Adresse Source : %02x:%02x:%02x:%02x:%02x:%02x\n", payload[10], payload[11], payload[12], payload[13], payload[14], payload[15]);
        printf("Adresse BSSID : %02x:%02x:%02x:%02x:%02x:%02x\n", payload[16], payload[17], payload[18], payload[19], payload[20], payload[21]);

        // Si c'est un Beacon ou Probe Response, extraire le SSID
        if (subtype == 0x08 || subtype == 0x05) {  // Beacon ou Probe Response
            extract_ssid(payload, length);
        }
    }
}

// Fonction pour analyser les trames de données (par exemple, un EAPOL dans le cas d'un handshake WPA)
void analyze_data_frame(const uint8_t *payload, uint16_t length) {
    uint8_t frame_control = payload[0];
    uint8_t type = (frame_control >> 2) & 0x03;  // Type de trame
    uint8_t subtype = (frame_control >> 4) & 0x0F;  // Sous-type de trame

    // Afficher les trames EAPOL (handshake WPA)
    if (type == 0x02 && subtype == 0x08) {  // Data frame, subtype 0x08 (EAPOL)
        if (length > 34 && payload[30] == 0x88 && payload[31] == 0x8E) {
            ESP_LOGI(TAG, "Trame EAPOL détectée (HandShake WPA/WPA2)");

            // Afficher le payload EAPOL
            printf("EAPOL Payload : ");
            for (int i = 0; i < length && i < 64; i++) {
                printf("%02x ", payload[i]);
            }
            printf("\n\n");
        }
    }
}

// Fonction de callback pour la capture des trames
void promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *payload = pkt->payload;
    uint16_t length = pkt->rx_ctrl.sig_len;

    // Analyser les trames Beacon et Probe Response
    if (type == 0) {  // Type de trame management (Beacon, Probe Request/Response)
        analyze_beacon_or_probe(payload, length);
    }

    // Analyser les trames de données (pour capturer les handshakes WPA/WPA2)
    if (type == 2) {  // Type de trame data (Data)
        analyze_data_frame(payload, length);
    }
}

// Fonction pour initialiser le Wi-Fi en mode promiscuous
void wifi_init_promiscuous(int duration_seconds) {
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

    // Attendre pendant la durée spécifiée avant d'arrêter le sniffer
    vTaskDelay(pdMS_TO_TICKS(duration_seconds * 1000));

    // Arrêter le mode promiscuous
    stop_sniffer();
}

void stop_sniffer(void) {
    ESP_LOGI(TAG, "Arrêt du mode promiscuous Wi-Fi");
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
}

// Définition des arguments pour la commande sniffer
static struct {
    struct arg_int *timeout;  // Durée du sniffing
    struct arg_end *end;
} sniffer_args;

static int sniffer(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &sniffer_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, sniffer_args.end, argv[0]);
        return 1;
    }

    ESP_LOGI(TAG, "Sniffer actif avec une durée de %d secondes", sniffer_args.timeout->ival[0]);

    // Lancer le mode promiscuous pour la durée spécifiée
    wifi_init_promiscuous(sniffer_args.timeout->ival[0]);

    return 0;
}

void module_sniff_wif(void)
{
    sniffer_args.timeout = arg_int1(NULL, NULL, "<t>", "Durée du sniffing en secondes");
    sniffer_args.end = arg_end(1);  // Fin des arguments

    const esp_console_cmd_t sniff_cmd = {
        .command = "sniffer_wifi",
        .help = "Démarre le mode promiscuous et affiche les trames Wi-Fi pendant une durée spécifiée",
        .hint = NULL,
        .func = &sniffer,
        .argtable = &sniffer_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&sniff_cmd));
}
