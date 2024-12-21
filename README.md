## RT-Thread ESP-Hosted

## Overview
The repository has adapted `ESP-Hosted-FG` to the RT-Thread system, currently only supports the SPI bus protocol, and uses the rt_spi_device of RT-Thread for SPI bus operations.<br>

This version of ESP-Hosted provides:
* A standard 802.3 network interface for transmitting and receiving 802.3 frames
* A standard HCI interface over which Bluetooth/BLE is supported
* A control interface to configure and control Wi-Fi on ESP board

ESP-Hosted-FG solution makes use of host's existing `TCP/IP and/or Bluetooth/BLE software stack` and `hardware peripheral like SPI/SDIO/UART` to connect to ESP firmware with very thin layer of software.

Although the project doesn't provide a standard 802.11 interface to the host, it provides a easy way, *i.e.* [control path](docs/common/contrl_path.md), to configure Wi-Fi. For the control path between the host and ESP board, ESP-Hosted-FG makes use of [Protobuf](https://developers.google.com/protocol-buffers), which is a language independent data serialization mechanism.

Details about ESP-Hosted-FG can be found in the [ESP-Hosted README](./docs/README.md).<br>

## Using

### Add this repository
- Clone the repository to the `packages` or `libraries` directory in the RT-Thread project.
- Because the `esp-idf` and `protobuf-c` is a submodule, you will need to clone with the --recursive option.
- In the `libraries` or `packages` folder in the RT-Thread project, include `Kconfig` file for `ESP-Hosted` in its Kconfig files.
- For example, include `ESP-Hosted` in the `libraries` directory:
```Kconfig
menu "External Libraries"
    source "$RTT_DIR/../libraries/rt-thread_esp-hosted/Kconfig"
endmenu
```

### Configure ESP-Hosted
- Use the `menuconfig` command in the env window
- Select Open `Using esp-hosted for espressif` 
```
→ External Libraries
     [*] Using esp-hosted for espressif  --->
```
- Enter `Using esp-hosted for espressif` menu to configure the pin and SPI bus:
```
--- Using esp-hosted for espressif
(8)   The priority of the esp-hosted thread
(5120) The stack size of the esp-hosted thread
(20)  The priority of the esp-hosted SPI thread
(1024) The stack size of the esp-hosted SPI thread
(2)   The size for esp-hosted SPI queue
(esp-hosted) Set the spi device name
(spi1)  Set the spi bus name (NEW)
(-1)    Set the SPI CS pin
(30000000) Set the maximum spi frequency(Hz)
(-1)  Set the data ready pin
(-1)  Set the handshake pin
(-1)  Set the reset pin
[*]   Use thread initialization
(2048)  The stack size of the init thread
(20)    The priority of the init thread
```

## Hardware connections for ESP32
| Function  | ESP32 Pin | ESP32-S2/S3 | ESP32-C2/C3/C5/C6 |
|-----------|-----------|-------------|----------------|
| MISO      | IO19      | IO13        | IO2            |
| CLK       | IO18      | IO12        | IO6            |
| MOSI      | IO23      | IO11        | IO7            |
| CS        | IO5       | IO10        | IO10           |
| GND       | GND       | GND         | GND            |
| Handshake | IO2       | IO2         | IO3            |
| Data Ready| IO4       | IO4         | IO4            |
| Reset ESP | EN        | RST         | RST            |

If you are familiar with esp-idf, you can also try to modify the pins

## Build ESP32 firmware
### Set-up ESP-IDF
If you already have an `esp-idf` environment, you can skip this `Set-up`<br>
But I recommend using the `release/v5.3` branch because this project uses it

- Install the ESP-IDF using script
```sh
$ cd esp/esp_driver
$ cmake .
```
- Set-Up the build environment using
```sh
$ . ./esp-idf/export.sh
# Optionally, You can add alias for this command in ~/.bashrc for later use
```

### Configure, Build & Flash ESP firmware
- Set slave chipset environment
```
$ cd network_adapter
$ rm -rf sdkconfig build
$ idf.py set-target <esp_chipset>
```
where <esp_chipset> could be one from "esp32", "esp32s2", "esp32s3", "esp32c2", "esp32c3", "esp32c5", "esp32c6"

- Execute following command to configure the project
```sh
$ idf.py menuconfig
```
- This will open project configuration window. To select SPI transport interface, navigate to `Example Configuration ->  Transport layer -> SPI interface -> select` and exit from menuconfig.

:warning: Skip below step for ESP32-S2/S3/C2/C3/C5/C6. Run for  ESP32 only.

- If ESP32 slave, Change SPI controller to VSPI. Please navigate to `Example Configuration → SPI Configuration` and change value of `SPI controller to use` to `3`

To build and flash the app on ESP peripheral, run

```sh
$ idf.py -p <serial_port> build flash
```

- Collect the firmware log using
```sh
$ idf.py -p <serial_port> monitor
```

## Checking the Setup

- Firmware log
On successful flashing, you should see following entry in ESP log:
```
I (412) NETWORK_ADAPTER: *********************************************************************
I (422) NETWORK_ADAPTER:                 ESP-Hosted Firmware version :: FG-0.0.6.0.0
I (432) NETWORK_ADAPTER:                 Transport used :: SPI only
I (442) NETWORK_ADAPTER: *********************************************************************
```

- Host log
you should see following entry in MCU log:
```shell
 \ | /
- RT -     Thread Operating System
 / | \     5.0.2 build Dec 20 2024 10:02:05
 2006 - 2022 Copyright by RT-Thread team
lwIP-2.1.2 initialized!
[4] I/SFUD: Found a Winbond flash chip. Size is 16777216 bytes.
[11] I/SFUD: norflash flash device initialized successfully.
[17] I/SFUD: Probe SPI flash norflash by SPI device spi40 success.
[I/FAL] RT-Thread Flash Abstraction Layer initialize success.
[47] I/sal.skt: Socket Abstraction Layer initialize success.
[I/FAL] The FAL block device (filesystem) created successfully
[238] I/SDIO: SD card capacity 31166976 KB.
found part[0], begin: 16384, size: 29.740GB
[258] I/filesystem: sd card mount to '/sdcard'
[907] I/esp.netdev: Chip is: ESP32c3
[911] I/esp.netdev: Features supported are:
[917] I/esp.netdev:      * WLAN
[922] I/esp.netdev:      * BT/BLE
[926] I/esp.netdev:        - HCI over SPI
[930] I/esp.netdev:        - BLE only
[1934] I/WLAN.dev: wlan init success
[1944] I/WLAN.lwip: eth device init ok name:w0
[1949] I/WLAN.dev: wlan init success
[1960] I/WLAN.lwip: eth device init ok name:w1
msh />[10856] I/WLAN.mgnt: wifi connect success ssid:TP-LINK_86A9
[11863] I/WLAN.lwip: Got IP address : 192.168.0.105
[30173] I/ntp: Get local time from NTP server: Fri Dec 20 15:13:16 2024
```

## Supported Chip

| **CHIP**  |**SDIO**|**SPI**|
|-----------|--------|-------|
| ESP32     |   x    |   *   |
| ESP32-C6  |   x    |   *   |
| ESP32-C5  |   x    |   *   |
| ESP32-C3  |   x    |   o   |
| ESP32-C2  |   x    |   *   |
| ESP32-S3  |   x    |   *   |
| ESP32-S2  |   x    |   *   |

'x' indicates no support<br>
'o' indicates tested and supported<br>
'*' means theoretically supported, but not tested

## More information
* [esp-hosted](https://github.com/espressif/esp-hosted)
* [espressif](https://www.espressif.com.cn)
