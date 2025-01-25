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
#include "cmd_arpscan.h"
#include <stdio.h>
#include <inttypes.h>

// Define
#define ARPTIMEOUT 5000
//Device Struct
typedef struct deviceInfo{
    int online;
    uint32_t ip;
    uint8_t mac[6];
} deviceInfo;

const char *TAG = "ARP SCAN";
// functions
uint32_t switch_ip_orientation (uint32_t *);
void nextIP(esp_ip4_addr_t *);

// storing
uint32_t deviceCount = 0; // store the exact online device count after the loop
uint32_t maxSubnetDevice = 0; // store the maxium device that subnet can hold

deviceInfo *deviceInfos; // store all ip, MAC, and online information to device


uint32_t switch_ip_orientation (uint32_t *ipv4){
    uint32_t ip =   ((*ipv4 & 0xff000000) >> 24)|\
                    ((*ipv4 & 0xff0000) >> 8)|\
                    ((*ipv4 & 0xff00) << 8)|\
                    ((*ipv4 & 0xff) << 24);
    return ip;
}

// get the next ip in numerical order
void nextIP(esp_ip4_addr_t *ip){
    // reconstruct it to normal order
    esp_ip4_addr_t normal_ip;
    normal_ip.addr = switch_ip_orientation(&ip->addr); // switch to the normal way

    // check if ip is the last ip in subnet
    if(normal_ip.addr == UINT32_MAX) return;

    // add one to obtain the next ip address location
    normal_ip.addr += (uint32_t)1;
    ip->addr = switch_ip_orientation(&normal_ip.addr); // switch back the lwip way

    return;
}


/* values */

// get subnet max device
uint32_t getMaxDevice(){
    return maxSubnetDevice;
}

// get device count
uint32_t getDeviceCount(){
    return deviceCount;
}

// get database
deviceInfo * getDeviceInfos(){
    return deviceInfos;
}

int arpScan(void) {
    /* Initialize...  */
    ESP_LOGI(TAG, "Starting ARP scan");

    // get esp_netif
    esp_netif_t* esp_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    // get netif from esp_netif
    struct netif *netif = (struct netif *)esp_netif_get_netif_impl(esp_netif);
    ESP_LOGI(TAG, "Got netif for lwip");

    // get internet information: ip, gateway, mask
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif, &ip_info);

    // get subnet range
    // uint32_t ip representatione
    esp_ip4_addr_t target_ipp;
    target_ipp.addr = ip_info.netmask.addr & ip_info.ip.addr; // get the first ip of subnet

    // calculate subnet max device count
    uint32_t normal_mask = switch_ip_orientation(&ip_info.netmask.addr);
    maxSubnetDevice = UINT32_MAX - normal_mask - 1; // the total count of ips to scan

    // initialize device information storing database
    deviceInfos = calloc(maxSubnetDevice, sizeof(deviceInfo));
    if(deviceInfos == NULL){
        ESP_LOGI(TAG, "Not enough space for storing information");
        return 1;
    }

    /* Loop start... */
    // scanning all ips from local network
    
    uint32_t onlineDevicesCount = 0;

    ESP_LOGI(TAG,"%" PRIu32 " ips to scan", maxSubnetDevice);

    // ips
    char char_target_ip[IP4ADDR_STRLEN_MAX]; // char ip for printing
    esp_ip4_addr_t target_ip, last_ip;
    target_ip.addr = target_ipp.addr; // first ip in subnet
    last_ip.addr = (target_ipp.addr)|(~ip_info.netmask.addr); // calculate last ip in subnet

    // scan loop for 5 at a time
    while(target_ip.addr != last_ip.addr){
        esp_ip4_addr_t currAddrs[5]; // save current loop ip
        int currCount = 0; // for checking Arp table use

        // send 5 ARP request a time because ARP table has limit size
        for(int i=0; i<5; i++){
            nextIP(&target_ip); // next ip
            if(target_ip.addr != last_ip.addr){
                esp_ip4addr_ntoa(&target_ip, char_target_ip, IP4ADDR_STRLEN_MAX);
                currAddrs[i] = target_ip;
                etharp_request(netif, &target_ip);
                ESP_LOGI(TAG, "Success sending ARP to %s", char_target_ip);
                currCount++;
            }
            else break; // ip is last ip in subnet then break
        }
        // wait for resopnd
        vTaskDelay(ARPTIMEOUT / portTICK_PERIOD_MS);

        // find received ARP resopnd in ARP table
        for(int i=0; i<currCount; i++){
            ip4_addr_t *ipaddr_ret = NULL;
            struct eth_addr *eth_ret = NULL;
            char mac[20], char_currIP[IP4ADDR_STRLEN_MAX];

            unsigned int currentIpCount = switch_ip_orientation(&currAddrs[i].addr) - switch_ip_orientation(&target_ipp.addr) - 1; // calculate No. of ip
            if(etharp_find_addr(NULL, &currAddrs[i], &eth_ret, &ipaddr_ret) != -1){ // find in ARP table
                // print MAC result for ip
                sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",eth_ret->addr[0],eth_ret->addr[1],eth_ret->addr[2],eth_ret->addr[3],eth_ret->addr[4],eth_ret->addr[5]);
                esp_ip4addr_ntoa(&currAddrs[i], char_currIP, IP4ADDR_STRLEN_MAX);
                ESP_LOGI(TAG, "%s's MAC address is %s", char_currIP, mac);
                onlineDevicesCount++;
            }
        }        
    }

    // update deviceCount
    deviceCount = onlineDevicesCount;
    // print network scanning result
    ESP_LOGI(TAG, "%" PRIu32 " devices are on local network", onlineDevicesCount);
    return 0;
}




void register_arp_scan(void)
{

    const esp_console_cmd_t arp_cmd = {
        .command = "scan-arp",
        .help = "Please Be connect to start this command",
        .hint = NULL,
        .func = &arpScan
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&arp_cmd) );
}
