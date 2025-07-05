## RT-Thread ESP-Hosted

[中文](./README_CN.md) | English

The project is licensed under the `MIT license` as a whole, but contains third-party code under other licenses. Please read carefully the following descriptions of the different license codes.

## Overview
The repository has adapted `ESP-Hosted-MCU` to the `RT-Thread` system, currently only supports the SPI bus protocol, and uses the `SPI Device` of `RT-Thread` for SPI bus operations.<br>

This version of ESP-Hosted provides:
* A standard 802.3 network interface for transmitting and receiving 802.3 frames
* A standard HCI interface over which Bluetooth/BLE is supported
* A control interface to configure and control Wi-Fi on ESP chips
* A OTA interface for upgrading ESP firmware

ESP-Hosted-MCU solution makes use of host's existing `TCP/IP and/or Bluetooth/BLE software stack` and `hardware peripheral like SPI/SDIO/UART` to connect to ESP firmware with very thin layer of software.

Although the project doesn't provide a standard 802.11 interface to the host, it provides a easy way, *i.e.* Remote Procedure Calls (RPCs), to configure Wi-Fi. For the RPC between the host and ESP board, ESP-Hosted-MCU makes use of [Protobuf](https://developers.google.com/protocol-buffers), which is a language independent data serialization mechanism.

Details about ESP-Hosted-MCU can be found in the [ESP-Hosted README](./esp-hosted/README.md).<br>

## Using

### Add this repository
- Clone the repository to the `packages` or `libraries` directory in the RT-Thread project.
- In the `libraries` or `packages` folder in the RT-Thread project, include `Kconfig` file for `ESP-Hosted` in its Kconfig files.
- For example, include `ESP-Hosted` in the `libraries` directory:
```Kconfig
menu "External Libraries"
    source "$RTT_DIR/../libraries/rt-thread_esp-hosted/Kconfig"
endmenu
```

### Configure ESP-Hosted
- Use the `menuconfig` command in the env window
- Select the `Using esp-hosted for espressif` 
```
→ External Libraries
     [*] Using esp-hosted for espressif  --->
```
- Enter `Using esp-hosted for espressif` menu to configure the ESP-Hoseted:
```
--- Using esp-hosted for espressif
    ESP-Hosted Configure  --->
        Select the transport interface (SPI)  --->
        Slave chipset to be used (Slave as ESP32C6)  --->
        [ ] Enable raw throughput transport  ----
        [ ] Enable Transport level packet statistics
        (8) The maximum number of simultaneous sync rpc requests
        (8) The maximum number of simultaneous async rpc requests
        (20) The priority of the esp-hosted rpc thread
        (5120) The stack size of the esp-hosted rpc thread
        (20) The priority of the esp-hosted transport thread
        (1024) The stack size of the esp-hosted SPI thread
        (8) The number for esp-hosted SPI queue
        (esp-hosted) Set the spi device name
        (spi1) Set the spi bus name
        (30000000) Set the maximum spi frequency(Hz)
            Select the pin name or number (Name)  --->
        (PE.7) Set the SPI CS pin name
        (PE.5) Set the data ready pin name
        (PE.6) Set the handshake pin name
        (PE.4) Set the reset pin name
        [*] Use thread initialization
        (2048) The stack size of the init thread
        (20)  The priority of the init thread
    [*] Enable Bluetooth  --->
        Select hci interface (Using vhci device drivers)  --->
        (vhci) The vhci device name
    Wi-Fi Configure  --->
        (40) Max number of WiFi static RX buffers
        (60) Max number of WiFi dynamic RX buffers
        Type of WiFi TX buffers (Dynamic)  --->
        (16) Max number of WiFi cache TX buffers
        (40) Max number of WiFi dynamic TX buffers
        [ ] WiFi CSI(Channel State Information)
        [ ] WiFi AMPDU TX
        [ ] WiFi AMPDU RX
        (752) Max length of WiFi SoftAP Beacon
        (32) WiFi mgmt short buffer number
        [*] Enable WPA3-Personal
        [ ] WiFi FTM
        [*] Power Management for station at disconnected
        [ ] WiFi GCMP Support(GCMP128 and GCMP256)
        [ ] WiFi GMAC Support(GMAC128 and GMAC256)
        (7) Maximum espnow encrypt peers number
        [ ] Enable 802.11R (Fast Transition) Support
```
`ESP-Hosted Configure` menu is used to configure the `transport interface`, `slave chipset` and `transport interface` related pins.<br>

`Select hci interface` option is used to select the `vhci device driver` or `NimBLE hci driver` for HCI interface.<br>
Selecting `vhci device driver` will create a `character device` which will emulate the HCI interface.<br>
Selecting `NimBLE hci driver` will directly connect to `NimBLE` stack.<br>

`Wi-Fi Configure` menu is used to configure the `ESP32` `Wi-Fi` parameters.<br>
`WiFi AMPDU TX` and `WiFi AMPDU RX` options are recommended to be disabled, as it is found that enabling them will cause Wi-Fi to drop packets, possibly due to the router not following the standard protocol.

## Hardware connections
| Signal      | ESP32 | ESP32-C2/C3/C5/C6 | ESP32-S2/S3 |
|-------------|-------|-------------------|-------------|
| CLK         | 14    | 6                 | 12          |
| MOSI        | 13    | 7                 | 11          |
| MISO        | 12    | 2                 | 13          |
| CS          | 15    | 10                | 10          |
| Handshake   | 26    | 3                 | 17          |
| Data Ready  | 4     | 4                 | 5           |
| Reset In    | EN    | EN/RST            | EN/RST      |

If you are familiar with esp-idf, you can also try to modify the pins<br>
Or use the [Flash Download Tool](https://dl.espressif.com/public/flash_download_tool.zip) to burn the firmware corresponding to the chip in the `firmware` directory.

## Build ESP32 firmware
```
$ cd esp-hosted/slave
$ rm -rf sdkconfig build
$ idf.py set-target <esp_chipset>
```
where <esp_chipset> could be one from "esp32", "esp32s2", "esp32s3", "esp32c2", "esp32c3", "esp32c5", "esp32c6"

- Execute following command to configure the project
```sh
$ idf.py menuconfig
```
- This will open project configuration window. To select SPI transport interface, navigate to `ESP-Hosted Configuration ->  Transport layer -> SPI interface -> select` and exit from menuconfig.

- To build and flash the app on ESP peripheral, run
```sh
$ idf.py -p <serial_port> build flash
```

- Collect the firmware log using
```sh
$ idf.py -p <serial_port> monitor
```

## Checking the Setup

#### Slave log

On successful flashing, you should see following entry in ESP log:
```
I (412) NETWORK_ADAPTER: *********************************************************************
I (422) NETWORK_ADAPTER:                 ESP-Hosted-MCU Slave FW version :: 1.0.0
I (432) NETWORK_ADAPTER:                 Transport used :: SPI only
I (442) NETWORK_ADAPTER: *********************************************************************
```

#### Host log

you should see following entry in host log:
```shell
 \ | /
- RT -     Thread Operating System
 / | \     5.0.2 build Feb 15 2025 09:39:29
 2006 - 2022 Copyright by RT-Thread team
lwIP-2.1.2 initialized!
[4] I/SFUD: Found a Winbond flash chip. Size is 16777216 bytes.
[9] I/SFUD: norflash flash device initialized successfully.
[14] I/SFUD: Probe SPI flash norflash by SPI device spiflash success.
[I/FAL] RT-Thread Flash Abstraction Layer initialize success.
[39] I/sal.skt: Socket Abstraction Layer initialize success.
[I/FAL] The FAL block device (filesystem) created successfully
[154] I/SDIO: SD card capacity 31166976 KB.
found part[0], begin: 2097152, size: 29.738GB
[167] I/filesystem: sd card mount to '/sdcard'
[1265] I/transport: Features supported are:
[1266] I/transport:        - WLAN over SPI
[1269] I/transport:        - HCI over SPI
[1273] I/transport:        - BLE only
[1276] I/transport: Chip is: ESP32c6
[1279] I/vhci.dev: Host BT Support: Enabled
[1283] I/vhci.dev: BT Transport Type: vhci devices
[2387] I/WLAN.dev: wlan init success
[2424] I/WLAN.lwip: eth device init ok name:w0
[2426] I/WLAN.dev: wlan init success
[2444] I/WLAN.lwip: eth device init ok name:w1
```

## Firmware update

`ESP-Hosted` supports upgrading the `ESP32` firmware through the transmission interface.<br>
The upgrade process is as follows:
```shell
msh />esp_ota /flash/network_adapter.bin
esp-hosted ota update started
erasing the ota partition..
writing: [==================================================] 100%  1084496 | 100 KB/s
firmware write success!!
slave will restart after 5 sec
```

The `esp_ota` command can support either `online download` or `local file` as the firmware upgrade package.<br>
The `online download` method requires the use of the `webclient` software package:
```shell
msh />esp_ota https://file.server.com/network_adapter.bin
esp-hosted ota update started
erasing the ota partition..
writing: [==================================================] 100%  1084496 | 81 KB/s
firmware write success!!
slave will restart after 5 sec
msh />
```

## Supported Chip

| **CHIP**  |**SDIO**|**SPI**|
|-----------|--------|-------|
| ESP32     |   x    |   *   |
| ESP32-C6  |   x    |   o   |
| ESP32-C5  |   x    |   *   |
| ESP32-C3  |   x    |   o   |
| ESP32-C2  |   x    |   o   |
| ESP32-S3  |   x    |   *   |
| ESP32-S2  |   x    |   *   |

'x' indicates no support<br>
'o' indicates tested and supported<br>
'*' means theoretically supported, but not tested

## More information
* [esp-hosted](https://github.com/espressif/esp-hosted)
* [esp-hosted-mcu](https://github.com/espressif/esp-hosted-mcu)
* [espressif](https://www.espressif.com.cn)
