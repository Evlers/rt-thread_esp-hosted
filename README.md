## RT-Thread ESP-Hosted

### Overview
The repository has adapted `ESP-Hosted-FG` to the RT-Thread system, currently only supports the SPI bus protocol, and uses the rt_spi_device of RT-Thread for SPI bus operations.<br>

This version of ESP-Hosted provides:
* A standard 802.3 network interface for transmitting and receiving 802.3 frames
* A standard HCI interface over which Bluetooth/BLE is supported
* A control interface to configure and control Wi-Fi on ESP board

ESP-Hosted-FG solution makes use of host's existing `TCP/IP and/or Bluetooth/BLE software stack` and `hardware peripheral like SPI/SDIO/UART` to connect to ESP firmware with very thin layer of software.

Although the project doesn't provide a standard 802.11 interface to the host, it provides a easy way, *i.e.* [control path](docs/common/contrl_path.md), to configure Wi-Fi. For the control path between the host and ESP board, ESP-Hosted-FG makes use of [Protobuf](https://developers.google.com/protocol-buffers), which is a language independent data serialization mechanism.

Details about ESP-Hosted-FG can be found in the [ESP-Hosted README](./docs/README.md).<br>

### Using

#### Add this repository
- Clone the repository to the `packages` or `libraries` directory in the RT-Thread project.
- Because the `esp-idf` and `protobuf-c` is a submodule, you will need to clone with the --recursive option.
- In the `libraries` or `packages` folder in the RT-Thread project, include `Kconfig` file for `ESP-Hosted` in its Kconfig files.
- For example, include `ESP-Hosted` in the `libraries` directory:
```Kconfig
menu "External Libraries"
    source "$RTT_DIR/../libraries/rt-thread_esp-hosted/Kconfig"
endmenu
```

#### Configure ESP-Hosted
- Use the `menuconfig` command in the env window
- Select Open `Using esp-hosted form espressif` 
```
→ External Libraries
     [*] Using esp-hosted form espressif  --->
```
- Enter `Using esp-hosted form espressif` menu to configure the pin and SPI bus:
```
--- Using esp-hosted form espressif
(8)   The priority of the esp-hosted thread
(4096) The stack size of the esp-hosted thread
(20)  The priority of the esp-hosted SPI thread
(512) The stack size of the esp-hosted SPI thread
(2)   The size for esp-hosted SPI queue
[*]   Initialize WiFi using the sample
(spi1)  Set the spi bus name (NEW)
(28)    Set the SPI CS pin
(esp-hosted) Set the spi device name
(25000000) Set the maximum spi frequency(Hz)
(39)  Set the data ready pin
(40)  Set the handshake pin
(38)  Set the reset pin
[*]   Use thread initialization
(2048)  The stack size of the init thread
(20)    The priority of the init thread
```
- If you do not use `sample` to initialize WiFi, copy `esp-hosted_port.c` in the `smaple` folder to the `board` folder for modification and add it to the project
- Example of ART-PI(STM32H750):
```
/*
 * Copyright (c) 2006-2024, Evlers Developers
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date         Author      Notes
 * 2024-01-11   Evlers      first implementation
 */

#include <rtthread.h>

#if defined(RT_USING_ESP_HOSTED) && !defined(ESP_HOSTED_USING_SAMPLE)
#include <rtdevice.h>
#include <drv_spi.h>
#include <board.h>

#define ESP_HOSTED_SPI_BUS_NAME             "spi2"
#define ESP_HOSTED_SPI_CS_PORT              GPIOI
#define ESP_HOSTED_SPI_CS_PIN               GPIO_PIN_0

extern int rt_hw_esp_wlan_init (void);

static void esp_hosted_init (void *parameter)
{
    /* reset esp32 chip */
    rt_pin_mode(ESP_HOSTED_RESET_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(ESP_HOSTED_RESET_PIN, PIN_LOW);
    rt_thread_mdelay(50);
    rt_pin_write(ESP_HOSTED_RESET_PIN, PIN_HIGH);
    
    /* stop spi transactions short time to avoid slave sync issues */
	rt_thread_mdelay(50);

    /* attach spi device */
    rt_hw_spi_device_attach(ESP_HOSTED_SPI_BUS_NAME, ESP_HOSTED_SPI_DEVICE_NAME,
							ESP_HOSTED_SPI_CS_PORT,
							ESP_HOSTED_SPI_CS_PIN);

    /* Initialize the esp-hosted */
    rt_hw_esp_wlan_init();
}

int esp_spi_device_init (void)
{
#ifdef ESP_HOSTED_THREAD_INIT
    /* Use thread initialization */
    rt_thread_t init_thread = rt_thread_create("esp_init", esp_hosted_init, NULL, 
                                                ESP_HOSTED_INIT_THREAD_STACK_SIZE,
						ESP_HOSTED_INIT_THREAD_PRIORITY, 20);
    RT_ASSERT(init_thread != NULL);
    rt_thread_startup(init_thread);
#else
    /* Thread initialization is not used */
    esp_hosted_init(NULL);
#endif

    return RT_EOK;
}
INIT_APP_EXPORT(esp_spi_device_init);


#endif /* defined(RT_USING_ESP_HOSTED) && !defined(ESP_HOSTED_USING_SAMPLE) */
```

#### Hardware connections for ESP32
| Function  | ESP32 Pin | ESP32-S2/S3 | ESP32-C2/C3/C6 |
|-----------|-----------|-------------|----------------|
| MISO      | IO19      | IO13        | IO2            |
| CLK       | IO18      | IO12        | IO6            |
| MOSI      | IO23      | IO11        | IO7            |
| CS        | IO5       | IO10        | IO10           |
| GND       | GND       | GND         | GND            |
| Handshake | IO2       | IO2         | IO3            |
| Data Ready| IO4       | IO4         | IO4            |
| Reset ESP | EN        | RST         | RST            |

#### Build esp32 firmware
##### Set-up ESP-IDF
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

##### Configure, Build & Flash ESP firmware
- Set slave chipset environment
```
$ cd network_adapter
$ rm -rf sdkconfig build
$ idf.py set-target <esp_chipset>
```
where <esp_chipset> could be one from "esp32", "esp32s2", "esp32s3", "esp32c2", "esp32c3", "esp32c6"

- Execute following command to configure the project
```sh
$ idf.py menuconfig
```
- This will open project configuration window. To select SPI transport interface, navigate to `Example Configuration ->  Transport layer -> SPI interface -> select` and exit from menuconfig.

:warning: Skip below step for ESP32-S2/S3/C2/C3/C6. Run for  ESP32 only.

- If ESP32 slave, Change SPI controller to VSPI. Please navigate to `Example Configuration → SPI Configuration` and change value of `SPI controller to use` to `3`

To build and flash the app on ESP peripheral, run

```sh
$ idf.py -p <serial_port> build flash
```

- Collect the firmware log using
```sh
$ idf.py -p <serial_port> monitor
```

#### Checking the Setup

- Firmware log
On successful flashing, you should see following entry in ESP log:
```
I (412) NETWORK_ADAPTER: *********************************************************************
I (422) NETWORK_ADAPTER:                 ESP-Hosted-FG Firmware version :: 0.0.5
I (432) NETWORK_ADAPTER:                 Transport used :: SPI only
I (442) NETWORK_ADAPTER: *********************************************************************
```

- Host log
you should see following entry in MCU log:
```
 \ | /
- RT -     Thread Operating System
 / | \     5.0.0 build Jan  8 2024 16:11:56
 2006 - 2022 Copyright by RT-Thread team
lwIP-2.0.3 initialized!
[I/sal.skt] Socket Abstraction Layer initialize success.
[I/esp.trans] Chip is: ESP32c3
[I/esp.trans] Features supported are:
[I/esp.trans]    * WLAN
[I/esp.trans]    * BT/BLE
[I/esp.trans]      - HCI over SPI
[I/esp.trans]      - BLE only
[I/esp.wlan] Register the wlan device.
[I/WLAN.dev] wlan init success
[I/WLAN.lwip] eth device init ok name:w0
[I/WLAN.dev] wlan init success
[I/WLAN.lwip] eth device init ok name:w1
[I/FAL] RT-Thread Flash Abstraction Layer initialize success.
[I/FAL] The FAL block device (root) created successfully
[Flash] EasyFlash V4.1.0 is initialize success.
[Flash] You can get the latest version on https://github.com/armink/EasyFlash .
msh />[I/WLAN.mgnt] wifi connect success ssid:TP-LINK_86A9
[I/WLAN.lwip] Got IP address : 192.168.0.105
[I/ntp] Get local time from NTP server: Mon Jan  8 17:14:12 2024
```

### Supported Chip

| **CHIP**  |**SDIO**|**SPI**|
|-----------|--------|-------|
| ESP32     |   x    |   *   |
| ESP32-C6  |   x    |   *   |
| ESP32-S2  |   x    |   *   |
| ESP32-C3  |   x    |   o   |
| ESP32-C2  |   x    |   *   |
| ESP32-S3  |   x    |   *   |

'x' indicates no support<br>
'o' indicates tested and supported<br>
'*' means theoretically supported, but not tested

### More information
* [esp-hosted](https://github.com/espressif/esp-hosted)
* [espressif](https://www.espressif.com.cn)
