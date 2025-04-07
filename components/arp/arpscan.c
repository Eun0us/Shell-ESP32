#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_console.h"
#include "esp_event.h"
#include "esp_netif_net_stack.h"
#include "argtable3/argtable3.h"
#include "lwip/ip4_addr.h"
#include "lwip/etharp.h"
#include "lwip/ip_addr.h"
#include <stdio.h>
#include <inttypes.h>
#include "arpscan.h"

// Define
#define ARPTIMEOUT 5000
//Device Struct
typedef struct deviceInfo{
    int online;
    uint32_t ip;
    uint8_t mac[6];
} deviceInfo;

const char *TAG = "ARP SCAN";

// Functions
uint32_t switch_ip_orientation(uint32_t *);
void nextIP(esp_ip4_addr_t *);

// Storing
uint32_t deviceCount = 0; // store the exact online device count after the loop
uint32_t maxSubnetDevice = 0; // store the maximum device that subnet can hold
deviceInfo *deviceInfos; // store all ip, MAC, and online information

uint32_t switch_ip_orientation(uint32_t *ipv4){
    uint32_t ip =   ((*ipv4 & 0xff000000) >> 24)|\
                    ((*ipv4 & 0xff0000) >> 8)|\
                    ((*ipv4 & 0xff00) << 8)|\
                    ((*ipv4 & 0xff) << 24);
    return ip;
}

// Get the next IP in numerical order
void nextIP(esp_ip4_addr_t *ip){
    esp_ip4_addr_t normal_ip;
    normal_ip.addr = switch_ip_orientation(&ip->addr); // Switch to the normal way

    // Check if ip is the last ip in subnet
    if (normal_ip.addr == UINT32_MAX) return;

    // Add one to obtain the next IP address location
    normal_ip.addr += (uint32_t)1;
    ip->addr = switch_ip_orientation(&normal_ip.addr); // Switch back the lwIP way

    return;
}

/* Values */

// Get subnet max device
uint32_t getMaxDevice(){
    return maxSubnetDevice;
}

// Get device count
uint32_t getDeviceCount(){
    return deviceCount;
}

// Get device database
deviceInfo * getDeviceInfos(){
    return deviceInfos;
}

// ARP scan function
int arpScan(int argc, char **argv) {
    ESP_LOGI(TAG, "Starting ARP scan");

    // Get esp_netif
    esp_netif_t* esp_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    struct netif *netif = (struct netif *)esp_netif_get_netif_impl(esp_netif);
    ESP_LOGI(TAG, "Got netif for lwip");

    // Get IP information: IP, gateway, mask
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif, &ip_info);

    // Get subnet range
    esp_ip4_addr_t target_ipp;
    target_ipp.addr = ip_info.netmask.addr & ip_info.ip.addr; // Get the first IP of subnet

    // Calculate subnet max device count
    uint32_t normal_mask = switch_ip_orientation(&ip_info.netmask.addr);
    maxSubnetDevice = UINT32_MAX - normal_mask - 1; // The total count of IPs to scan

    // Initialize device information storing database
    deviceInfos = calloc(maxSubnetDevice, sizeof(deviceInfo));
    if(deviceInfos == NULL){
        ESP_LOGI(TAG, "Not enough space for storing information");
        return 1;
    }

    uint32_t onlineDevicesCount = 0;
    ESP_LOGI(TAG, "%" PRIu32 " ips to scan", maxSubnetDevice);

    // Ips
    char char_target_ip[IP4ADDR_STRLEN_MAX];
    esp_ip4_addr_t target_ip, last_ip;
    target_ip.addr = target_ipp.addr; // First IP in subnet
    last_ip.addr = (target_ipp.addr)|(~ip_info.netmask.addr); // Calculate last IP in subnet

    // Scan loop for 5 at a time
    while(target_ip.addr != last_ip.addr){
        esp_ip4_addr_t currAddrs[5]; // Save current loop IP
        int currCount = 0; // For checking ARP table use

        // Send 5 ARP requests at a time because ARP table has a limit size
        for(int i = 0; i < 5; i++){
            nextIP(&target_ip); // Get next IP
            if(target_ip.addr != last_ip.addr){
                esp_ip4addr_ntoa(&target_ip, char_target_ip, IP4ADDR_STRLEN_MAX);
                currAddrs[i] = target_ip;
                etharp_request(netif, (const ip4_addr_t *)&target_ip); // Cast for compatibility
                ESP_LOGI(TAG, "Success sending ARP to %s", char_target_ip);
                currCount++;
            }
            else break; // IP is last IP in subnet then break
        }

        // Wait for response
        vTaskDelay(ARPTIMEOUT / portTICK_PERIOD_MS);

        // Find received ARP response in ARP table
        for(int i = 0; i < currCount; i++){
            ip4_addr_t *ipaddr_ret = NULL;
            struct eth_addr *eth_ret = NULL;
            char mac[20], char_currIP[IP4ADDR_STRLEN_MAX];

            unsigned int currentIpCount = switch_ip_orientation(&currAddrs[i].addr) - switch_ip_orientation(&target_ipp.addr) - 1; // Calculate the number of IP
            if(etharp_find_addr(NULL, (const ip4_addr_t *)&currAddrs[i], &eth_ret, (const ip4_addr_t **)&ipaddr_ret) != -1){ // Find in ARP table
                // Print MAC result for IP
                sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", eth_ret->addr[0], eth_ret->addr[1], eth_ret->addr[2], eth_ret->addr[3], eth_ret->addr[4], eth_ret->addr[5]);
                esp_ip4addr_ntoa(&currAddrs[i], char_currIP, IP4ADDR_STRLEN_MAX);
                ESP_LOGI(TAG, "%s's MAC address is %s", char_currIP, mac);
                onlineDevicesCount++;
            }
        }        
    }

    // Update deviceCount
    deviceCount = onlineDevicesCount;
    // Print network scanning result
    ESP_LOGI(TAG, "%" PRIu32 " devices are on local network", onlineDevicesCount);

    // Free allocated memory
    free(deviceInfos);
    return 0;
}

// Register ARP scan command
void module_arp_scan(void)
{
    const esp_console_cmd_t arp_cmd = {
        .command = "scan-arp",
        .help = "Please be connected to start this command",
        .hint = NULL,
        .func = &arpScan,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&arp_cmd) );
}
