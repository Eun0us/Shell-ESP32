| Supported Targets | Every ESP32 |
-----------------------------------

## Overview

ESPILON is a shell interface for ESP32 that supports various commands for system management, Wi-Fi operations, and debugging. 
This project provides a prebuilt binary and a guide for building the firmware from source using ESP-IDF.



## Build

Require **ESP-IDF** (idf.py)

### Active bluetooth in menuconfig 

`idf.py menuconfig`

Component Config -> Bleutooth -> []**Active Bluetooth**
Component Config -> Enable UART -> CONFIG_ESP_CONSOLE_UART_DEFAULT

### Build
`idf.py flash`

### Flash
`idf.py flash`
*or*
`esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 write_flash -z 0x1000 build/espilon.bin`
### Monitor
`idf.py monitor`

idf.py menuconfig
Enable UART
CONFIG_ESP_CONSOLE_UART_DEFAULT

## Command
**Helper**

striker:> help
free 
  Get the current size of free heap memory

join  [--timeout=<t>] <ssid> [<pass>]
  Join WiFi AP as a station
  --timeout=<t>  Connection timeout, ms
        <ssid>  SSID of AP
        <pass>  PSK of AP

sniffer 
  Start promiscuous mode and display Wi-Fi frames

scan-wifi 
  scan all network around us

ping  [-W <t>] [-i <t>] [-s <n>] [-c <n>] [-Q <n>] [-T <n>] [-I <n>] <host>
  send ICMP ECHO_REQUEST to network hosts
  -W, --timeout=<t>  Time to wait for a response, in seconds
  -i, --interval=<t>  Wait interval seconds between sending each packet
  -s, --size=<n>  Specify the number of data bytes to be sent
  -c, --count=<n>  Stop after sending count packets
  -Q, --tos=<n>  Set Type of Service related bits in IP datagrams
  -T, --ttl=<n>  Set Time to Live related bits in IP datagrams
  -I, --interface=<n>  Set Interface number
        <host>  Host address

scan-arp 
  Please Be connect to start this command

help  [<string>]
  Print the summary of all registered commands if no arguments are given,
  otherwise print summary of given command.
      <string>  Name of command
```

### Connect 
In this image, the join command is used as follows:
`join <ssid> <pass>`
The connection initializes with the event *esp_netif_handlers*, assigning a DHCP lease in the network.
![alt text](img/wifi_join.png)

### WiFi-scanner & Sniffer 

The ESP32 sniffs Wi-Fi frames effectively!
![alt text](img/sniffer.png)


In this image, the network scan displays the following information:
`<SSID BSSID RSSI AUTHMODE CHANNEL>` 

![alt text](img/wifibind.png)

### ARP scan

The ARP scan returns devices on my LAN 192.168.1.0/24.
![alt text](img/arp_response.png)

### Ping
Ping command
```ping  [-W <t>] [-i <t>] [-s <n>] [-c <n>] [-Q <n>] [-T <n>] [-I <n>] <host>```
![alt text](img/ping.png)



## Author
- Eun0us
