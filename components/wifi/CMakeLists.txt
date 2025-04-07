idf_component_register(SRCS "scan_wifi.c" "join_wifi.c" "sniff_wifi.c"
                    INCLUDE_DIRS .
                    REQUIRES console esp_wifi)
