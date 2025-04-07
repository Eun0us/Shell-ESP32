idf_component_register(SRCS "arpscan.c"
                    INCLUDE_DIRS .
                    REQUIRES console esp_wifi lwip driver
                    PRIV_REQUIRES nvs_flash)
