idf_component_register(SRCS "ping.c" "proxy.c"
                    INCLUDE_DIRS .
                    REQUIRES console esp_wifi protocol_examples_common)
