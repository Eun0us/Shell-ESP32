idf_component_register(SRCS "cmd_wifiscan.c" "cmd_wifi.c" "cmd_sniff.c"
                    INCLUDE_DIRS .
                    REQUIRES console esp_wifi)
