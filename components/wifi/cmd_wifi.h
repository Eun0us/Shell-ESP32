#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Register WiFi functions
void register_join_wifi_cmd(void);
void module_scan_wifi(void);
void module_sniff_wif(void);
#ifdef __cplusplus
}
#endif
