## RT-Thread ESP-Hosted

中文 | [English](./README.md)

该项目在`MIT license`下作为一个整体，但包含第三方代码在其他许可下。请仔细阅读以下不同许可代码的说明。

## 概述
该存储库已将`ESP-Hosted-MCU`适配于`RT-Thread`系统，目前仅支持SPI总线协议，并使用`RT-Thread`的`SPI Device`进行SPI总线操作。<br>

这个版本的ESP-Hosted提供:
* 一个标准的802.3网络接口，用于发送和接收802.3帧
* 支持蓝牙/BLE的标准HCI接口
* ESP32芯片配置和控制Wi-Fi的控制接口
* 用于升级ESP固件的OTA接口

ESP-Hosted-MCU解决方案利用主机现有的`TCP/IP 和 蓝牙/BLE 协议栈` 和 `SPI/SDIO/UART等硬件外设`连接到ESP固件，软件层非常薄。

虽然这个项目没有为主机提供标准的802.11接口，但它提供了一种简单的方法，既Remote Procedure Calls(RPCs)用于配置Wi-Fi。对于主机和ESP板之间的RPC， ESP-Hosted-MCU使用了[Protobuf](https://developers.google.com/protocol-buffers)，这是一种独立于语言的数据序列化机制。

关于ESP-Hosted-MCU的详细信息可以在[ESP-Hosted README](./esp-hosted/README.md)中找到。<br>

## 使用

### 添加这个仓库
- 将存储库克隆到RT-Thread项目中的`packages` or `libraries`目录。
- 在RT-Thread项目的`libraries`或`packages`文件夹中，在其Kconfig文件中包含用于`ESP-Hosted`的`Kconfig`文件。
- 例如，将`ESP-Hosted`包含在`libraries`目录中：
```Kconfig
menu "External Libraries"
    source "$RTT_DIR/../libraries/rt-thread_esp-hosted/Kconfig"
endmenu
```

### 配置 ESP-Hosted
- 在env窗口中使用`menuconfig`命令
- 选中 `Using esp-hosted for espressif` 
```
→ External Libraries
     [*] Using esp-hosted for espressif  --->
```
- 进入`Using esp-hosted for espressif`菜单，配置esp-hosted：
```
--- Using esp-hosted for espressif
    ESP-Hosted Configure  --->                                      # ESP-Hosted 配置
        Select the transport interface (SPI)  --->                  # 选择传输接口
        Slave chipset to be used (Slave as ESP32C6)  --->           # 选择从机芯片
        [ ] Enable raw throughput transport  ----                   # 启用原始吞吐量传输
        [ ] Enable Transport level packet statistics                # 启用传输级别数据包统计
        (8) The maximum number of simultaneous sync rpc requests    # 同步RPC请求的最大数量
        (8) The maximum number of simultaneous async rpc requests   # 异步RPC请求的最大数量
        (20) The priority of the esp-hosted rpc thread              # RPC线程的优先级
        (5120) The stack size of the esp-hosted rpc thread          # RPC线程的堆栈大小
        (20) The priority of the esp-hosted transport thread        # 传输线程的优先级
        (1024) The stack size of the esp-hosted SPI thread          # SPI线程的堆栈大小
        (8) The number for esp-hosted SPI queue                     # SPI传输队列的数量
        (esp-hosted) Set the spi device name                        # SPI设备名称
        (spi1) Set the spi bus name                                 # SPI总线名称
        (30000000) Set the maximum spi frequency(Hz)                # SPI传输的最大频率
            Select the pin name or number (Name)  --->              # 选择引脚名称或编号
        (PE.7) Set the SPI CS pin name                              # SPI CS引脚名称
        (PE.5) Set the data ready pin name                          # 数据就绪引脚名称
        (PE.6) Set the handshake pin name                           # 握手引脚名称
        (PE.4) Set the reset pin name                               # 复位引脚名称
        [*] Use thread initialization                               # 使用线程初始化
        (2048) The stack size of the init thread                    # 初始化线程的堆栈大小
        (20)  The priority of the init thread                       # 初始化线程的优先级
    [*] Enable Bluetooth  --->                                      # 启用蓝牙/BLE HCI接口
        Select hci interface (Using vhci device drivers)  --->      # 选择HCI接口
        (vhci) The vhci device name                                 # vhci设备名称
    Wi-Fi Configure  --->                                           # Wi-Fi 配置
        (40) Max number of WiFi static RX buffers                   # 静态RX缓冲区最大数量
        (60) Max number of WiFi dynamic RX buffers                  # 动态RX缓冲区最大数量
        Type of WiFi TX buffers (Dynamic)  --->                     # WiFi TX缓冲区类型
        (16) Max number of WiFi cache TX buffers                    # WiFi TX缓存最大数量
        (40) Max number of WiFi dynamic TX buffers                  # TX动态缓冲区最大数量
        [ ] WiFi CSI(Channel State Information)                     # Wi-Fi CSI
        [ ] WiFi AMPDU TX                                           # AMPDU TX(数据聚合)
        [ ] WiFi AMPDU RX                                           # AMPDU RX(数据聚合)
        (752) Max length of WiFi SoftAP Beacon                      # SoftAP Beacon最大长度
        (32) WiFi mgmt short buffer number                          # WiFi mgmt short buffer数量
        [*] Enable WPA3-Personal                                    # WPA3-Personal
        [ ] WiFi FTM                                                # WiFi FTM
        [*] Power Management for station at disconnected            # Wi-Fi PM
        [ ] WiFi GCMP Support(GCMP128 and GCMP256)                  # Wi-Fi GCMP
        [ ] WiFi GMAC Support(GMAC128 and GMAC256)                  # Wi-Fi GMAC
        (7) Maximum espnow encrypt peers number                     # espnow加密对等数量
        [ ] Enable 802.11R (Fast Transition) Support                # 802.11R
```
`ESP-Hosted Configure`菜单中，主要是`传输接口`，`从机芯片`以及`传输以为的引脚`配置，其他的默认即可。<br>

`Select hci interface`选项用于选择使用`vhci设备驱动`或者`NimBLE hci 驱动`的HCI接口。<br>
选择`vhci设备驱动`将创建一个`字符类型`设备，该设备将模拟蓝牙HCI接口。<br>
选择`NimBLE hci 驱动`将直接接入`NimBLE`协议栈。<br>

`Wi-Fi Configure`菜单中，主要是关于`ESP32`的`WiFi`参数配置。<br>
`WiFi AMPDU TX`和`WiFi AMPDU RX`选项建议关闭，在测试中发现开启后，Wi-Fi容易丢包，可能是由于路由器不遵循标准协议导致。

## 硬件连接
| Signal      | ESP32 | ESP32-C2/C3/C5/C6 | ESP32-S2/S3 |
|-------------|-------|-------------------|-------------|
| CLK         | 14    | 6                 | 12          |
| MOSI        | 13    | 7                 | 11          |
| MISO        | 12    | 2                 | 13          |
| CS          | 15    | 10                | 10          |
| Handshake   | 26    | 3                 | 17          |
| Data Ready  | 4     | 4                 | 5           |
| Reset In    | EN    | EN/RST            | EN/RST      |

如果您熟悉esp-idf，您也可以尝试修改引脚

## 构建ESP32固件
```
$ cd esp-hosted/slave
$ rm -rf sdkconfig build
$ idf.py set-target <esp_chipset>
```
其中 <esp_chipset> 可以是 "esp32", "esp32s2", "esp32s3", "esp32c2", "esp32c3", "esp32c5", "esp32c6"

- 执行以下命令配置项目
```sh
$ idf.py menuconfig
```
- 这将打开项目配置窗口。 要选择SPI传输接口，导航到 `ESP-Hosted Configuration ->  Transport layer -> SPI interface -> select` 然后退出菜单配置。

- 要构建并烧录应用程序到ESP设备，请运行
```sh
$ idf.py -p <serial_port> build flash
```

- 收集ESP固件日志使用
```sh
$ idf.py -p <serial_port> monitor
```

## 检查启动

#### 从机日志

在成功烧录后，您应该在ESP日志中看到以下信息:
```
I (412) NETWORK_ADAPTER: *********************************************************************
I (422) NETWORK_ADAPTER:                 ESP-Hosted-MCU Slave FW version :: 1.0.0
I (432) NETWORK_ADAPTER:                 Transport used :: SPI only
I (442) NETWORK_ADAPTER: *********************************************************************
```

#### 主机日志

您应该在主机日志中看到以下信息:
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
[5954] I/WLAN.mgnt: wifi connect success ssid:EvlersHome
[6957] I/WLAN.lwip: Got IP address : 192.168.10.163
[10107] I/ntp: Get local time from NTP server: Sat Feb 15 10:34:22 2025
```

## 芯片支持

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

## 更多信息
* [esp-hosted](https://github.com/espressif/esp-hosted)
* [esp-hosted-mcu](https://github.com/espressif/esp-hosted-mcu)
* [espressif](https://www.espressif.com.cn)
