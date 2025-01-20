# Bluetooth Implementation in ESP-Hosted

**Table of Contents**

- [1. Introduction](#1-introduction)
  - [1.1 Choosing a Bluetooth Host stack](#11-choosing-a-bluetooth-host-stack)
- [2. Bluetooth Controller](#2-bluetooth-controller)
- [3. Bluetooth Interface](#3-bluetooth-interface)
- [4. NimBLE Host Stack](#4-nimble-host-stack)
  - [4.1. Transporting HCI data using vHCI in NimBLE](#41-transporting-hci-data-using-vhci-in-nimble)
    - [4.1.1. Bluetooth Host vHCI Initialization](#411-bluetooth-host-vhci-initialization)
    - [4.1.2. Bluetooth Host Sending Data through vHCI in NimBLE](#412-bluetooth-host-sending-data-through-vhci-in-nimble)
    - [4.1.3. Bluetooth Host Receiving Data from vHCI in NimBLE](#413-bluetooth-host-receiving-data-from-vhci-in-nimble)
  - [4.2. Transporting HCI data using UART](#42-transporting-hci-data-using-uart)
    - [4.2.1. Bluetooth Host HCI Initialization](#421-bluetooth-host-hci-initialization)
    - [4.2.2. Bluetooth Host Sending Data using HCI](#422-bluetooth-host-sending-data-using-hci)
    - [4.2.3. Bluetooth Host Receiving Data using HCI](#423-bluetooth-host-receiving-data-using-hci)
- [5. BlueDroid Host Stack](#5-bluedroid-host-stack)
  - [5.1. Transporting HCI data using vHCI in BlueDroid](#51-transporting-hci-data-using-vhci-in-bluedroid)
    - [5.1.1. Bluetooth Host vHCI Initialization](#511-bluetooth-host-vhci-initialization)
    - [5.1.2. Bluetooth Host Sending Data through vHCI in BlueDroid](#512-bluetooth-host-sending-data-through-vhci-in-bluedroid)
    - [5.1.3. Bluetooth Host Receiving Data from vHCI in BlueDroid](#513-bluetooth-host-receiving-data-from-vhci-in-bluedroid)
  - [5.2. Transporting HCI data using UART](#52-transporting-hci-data-using-uart)
    - [5.2.1. Bluetooth Host HCI Initialization](#521-bluetooth-host-hci-initialization)
    - [5.2.2. Bluetooth Host Sending Data using HCI](#522-bluetooth-host-sending-data-using-hci)
    - [5.2.3. Bluetooth Host Receiving Data using HCI](#523-bluetooth-host-receiving-data-using-hci)
- [6. References](#6-references)

## 1. Introduction

ESP-Hosted can transport Bluetooth HCI packets between the Bluetooth
Host on the Hosted Master and the Bluetooth Controller on the Hosted
Co-processor. The Host MCU implement the Bluetooth app and Bluetooth
Host Stack and the co-processor runs the Bluetooth controller and
hardware.

> [!NOTE]
> Check that the memory requirement for your preferred Bluetooth host
> stack can be satisfied on the Host.

ESP-Hosted has is Bluetooth stack agnostic. To showcase ESP-Hosted's
Bluetooth support, Both `esp-nimble` and `esp-bluedroid` are used
here. Users can use their own preferred Bluetooth stack with some
porting effort.

`esp-nimble` is a fork of Apache NimBLE and available from
ESP-IDF. The NimBLE Bluetooth slack proves Bluetooth Low Energy (BLE)
only functionality.

`esp-bluedroid` is a fork of the Bluedroid based stack and available
from ESP-IDF. The BlueDroid stack supports classic Bluetooth as well
as Bluetooth Low Energy.

See [References](#7-references) for links with more information.

### 1.1 Choosing a Bluetooth Host stack

For usecases involving classic Bluetooth as well as Bluetooth Low
Energy, BlueDroid should be used.

For Bluetooth Low Energy-only usecases, using NimBLE is
recommended. It is less demanding in terms of code footprint and
runtime memory, making it suitable for such scenarios.

## 2. Bluetooth Controller

ESP-Hosted uses the Bluetooth controller running on the
co-processor. The co-processor is expected to be configured to use
ESP-Hosted as the transport.

As ESP-Hosted is just the communication medium, it isn't limited to
BLE only. Classic BT stacks are also supported, if the co-processor
has a Classic-BT controller. The Classic-BT or BLE or both
availability depends upon the Bluetooth stack support and ESP chipset
chosen. As of today, ESP32 supports Classic-BT+BLE, whereas, the other
ESP chipsets support BLE only.

## 3. Bluetooth Interface

Hosted provides two ways to let the Bluetooth Host stack running on
the Host to communicate with the Bluetooth controller on the
co-processor.

**vHCI**

vHCI works in shared mode. Bluetooth traffic is multiplexed with other
traffic types on same bus, each with different Interface types. See
the [traffic types](../README.md#72-interface-types) support
by ESP-Hosted message headers.

- vHCI is standard HCI with extra headers or metadata added
- vHCI embeds the ESP-Hosted header and re-uses the underlying
  ESP-Hosted transport, such as SPI/SDIO
- this option is easier to set up. Once the existing ESP-Hosted
  Transport (SPI or SDIO, for example) has been set up, Bluetooth
  just works

Use this option if you want:

- complete control of Bluetooth messages
- extra flexibility of debugging
- no extra GPIOs (required for Standard HCI)

**Standard HCI**

Standard HCI works in dedicated mode. Bluetooth traffic cannot be
multiplexed with other traffic, so a dedicated transport is
needed. Bluetooth HCI frames (without any hosted header) are
transferred over the dedicated transport.

- standard HCI is a transparent way of handling HCI messages
- HCI messages originating from the Bluetooth stack on the Host are
  sent through an interface (like UART) directly to the Bluetooth
  controller on the co-processor.
- requires extra GPIO for the HCI interface, independent of the GPIOs
  used for the ESP-Hosted interface

Use this option if you want:

- transparency: no extra data added to the HCI messages
- portability: because it is standard HCI, you can replace the
  co-processor with any other co-processor (ESP or otherwise) that has
  a Bluetooth controller

> [!NOTE]
> If Hosted is configured as the Bluetooth transport (vHCI), then your
> Bluetooth over HCI configuration must be disabled, and vice
> versa.

## 4. NimBLE Host Stack

The ESP-Hosted Master implements the set of API calls required by the
NimBLE Bluetooth stack to initialize, send and receive Bluetooth data:

- `hci_drv_init`
- `ble_transport_ll_init`
- `ble_transport_to_ll_acl_impl`
- `ble_transport_to_ll_cmd_impl`
- `ble_transport_to_hs_evt`
- `ble_transport_to_hs_acl`

The following sequence diagrams show how to send and receive Bluetooth
on both the Hosted Master and co-processor.

### 4.1. Transporting HCI data using vHCI in NimBLE

#### 4.1.1. Bluetooth Host vHCI Initialization

```mermaid
sequenceDiagram
    box Grey Hosted Master
    participant ble as NimBLE Host Bluetooth Stack
    participant vhci as VHCI Driver
    participant master as SPI/SDIO Interface
    end

    box Grey Hosted Co-processor
    participant sinterface as SPI/SDIO Interface
    participant slave as Bluetooth Controller
    end

    ble ->> +vhci : hci_drv_init()
	Note over vhci: do any init required
    vhci -->> -ble : 

    ble ->> +vhci : ble_transport_ll_init()
	Note over vhci : do any transport init required
    vhci -->> -ble : 
```

**Bluetooth Host Initialization**

#### 4.1.2. Bluetooth Host Sending Data through vHCI in NimBLE

```mermaid
sequenceDiagram
    box Grey Hosted Master
    participant ble as NimBLE Host Bluetooth Stack
    participant vhci as VHCI Driver
    participant master as SPI/SDIO Interface
    end

    box Grey Hosted Co-processor
    participant sinterface as SPI/SDIO Interface
    participant slave as Bluetooth Controller
    end

    ble ->> +vhci : ble_transport_to_ll_acl_impl()
    Note over vhci : convert ACL data to HCI
    vhci ->> +master : esp_hosted_tx()
    Note over master : add Hosted header
    master ->> +sinterface: SPI/SDIO
    Note over master,sinterface : (VHCI data)
    master -->> -vhci :  
    vhci -->> -ble : 

    Note over sinterface : remove Hosted header
    sinterface ->> -slave : HCI data
```

**Bluetooth Host Sending Data**

#### 4.1.3. Bluetooth Host Receiving Data from vHCI in NimBLE

```mermaid
sequenceDiagram
    box Grey Hosted Master
    participant ble as NimBLE Host Bluetooth Stack
    participant vhci as VHCI Driver
    participant master as SPI/SDIO Interface
    end

    box Grey Hosted Co-processor
    participant sinterface as SPI/SDIO Interface
    participant slave as Bluetooth Controller
    end

    slave ->> +sinterface : HCI data
    Note over sinterface : Add Hosted header
    sinterface ->> -master : SPI/SDIO
    Note over sinterface,master : (VHCI data)
    Note over master : Remove Hosted header

    master ->> +vhci : hci_rx_handler()

    alt Receive Event Data
        Note over vhci: convert HCI data to Event
	    vhci ->> ble : ble_transport_to_hs_evt()
	    ble -->> vhci : 
    else Receive ACL Data
        Note over vhci: convert HCI data to ACL
	    vhci ->> ble : ble_transport_to_hs_acl()
	    ble -->> vhci : 
    end

    vhci -->> -master : 
```

**Bluetooth Host Receiving Data**

### 4.2. Transporting HCI data using UART

#### 4.2.1. Bluetooth Host HCI Initialization

```mermaid
sequenceDiagram
    box Grey Master
    participant ble as NimBLE Host Bluetooth Stack
    participant huart as UART Driver
    end

    box Grey Co-processor
    participant slave as Bluetooth Controller with UART Interface
    end

    ble ->> huart : hci_drv_init()
    Note over huart : do any init required
    huart -->> ble : 

    ble ->> huart : ble_transport_ll_init()
    Note over huart : do any transport init required
    huart --> ble : 
```

**Bluetooth Host Initialization**

#### 4.2.2. Bluetooth Host Sending Data using HCI

```mermaid
sequenceDiagram
    box Grey Master
    participant ble as NimBLE Host Bluetooth Stack
    participant huart as UART Driver
    end

    box Grey Co-processor
    participant slave as Bluetooth Controller with UART Interface
    end

    ble ->> huart : ble_transport_to_ll_acl_impl()
    Note over huart : convert ACL data to HCI
    huart ->> slave : UART TX 
    Note over huart,slave : (standard HCI)
    huart -->> ble : 
```

**Bluetooth Host Sending Data**

#### 4.2.3. Bluetooth Host Receiving Data using HCI

```mermaid
sequenceDiagram
    box Grey Master
    participant ble as NimBLE Host Bluetooth Stack
    participant huart as UART Driver
    end

    box Grey Co-processor
    participant slave as Bluetooth Controller with UART Interface
    end

    slave ->> huart : UART RX
    Note over slave,huart: (standard HCI)

    alt Receive Event Data
        Note over huart : convert HCI data to Event
	    huart ->> ble : ble_transport_to_hs_evt()
	    ble -->> huart : 
    else Receive ACL Data
        Note over huart : convert HCI data to ACL
	    huart ->> ble : ble_transport_to_hs_acl()
	    ble -->> huart : 
    end
```

**Bluetooth Host Receiving Data**

## 5. BlueDroid Host Stack

The ESP-Hosted Master implements the set of API calls required by the
BlueDroid Bluetooth stack to initialise, send and receive Bluetooth
data.

- `hosted_hci_bluedroid_open`
- `hosted_hci_bluedroid_close`
- `hosted_hci_bluedroid_send`
- `hosted_hci_bluedroid_check_send_available`
- `hosted_hci_bluedroid_register_host_callback`

`hosted_hci_bluedroid_open` must be called by the application before
attaching the transport APIs to BlueDroid and starting BlueDroid. This
initializes the underlying transport.

`hosted_hci_bluedroid_register_host_callback` records the callback provided by BlueDroid that is use to notify the Bluetooth stack of incoming HCI data (as `notify_host_recv`).

The following sequence diagrams show how to send and receive Bluetooth
on both the Hosted Master and Co-processor.

### 5.1. Transporting HCI data using vHCI in BlueDroid

#### 5.1.1. Bluetooth Host vHCI Initialization

```mermaid
sequenceDiagram
    box Grey Hosted Master
    participant bt as Host Application
    participant vhci as VHCI Driver
    participant master as SPI/SDIO Interface
    end

    box Grey Hosted Co-processor
    participant sinterface as SPI/SDIO Interface
    participant slave as Bluetooth Controller
    end

    bt ->> +vhci : hosted_hci_bluedroid_open()
	Note over vhci: do any init required
    vhci -->> -bt : 
```

**Bluetooth Host Initialization**

#### 5.1.2. Bluetooth Host Sending Data through vHCI in BlueDroid

```mermaid
sequenceDiagram
    box Grey Hosted Master
    participant bt as BlueDroid Host Bluetooth Stack
    participant vhci as VHCI Driver
    participant master as SPI/SDIO Interface
    end

    box Grey Hosted Co-processor
    participant sinterface as SPI/SDIO Interface
    participant slave as Bluetooth Controller
    end

    bt ->> +vhci : hosted_hci_bluedroid_send()
    Note over vhci : HCI data
    vhci ->> +master : esp_hosted_tx()
    Note over master : add Hosted header
    master ->> +sinterface: SPI/SDIO
    Note over master,sinterface : (VHCI data)
    master -->> -vhci :  
    vhci -->> -bt : 

    Note over sinterface : remove Hosted header
    sinterface ->> -slave : HCI data
```

**Bluetooth Host Sending Data**

#### 5.1.3. Bluetooth Host Receiving Data from vHCI in BlueDroid

```mermaid
sequenceDiagram
    box Grey Hosted Master
    participant bt as BlueDroid Host Bluetooth Stack
    participant vhci as VHCI Driver
    participant master as SPI/SDIO Interface
    end

    box Grey Hosted Co-processor
    participant sinterface as SPI/SDIO Interface
    participant slave as Bluetooth Controller
    end

    slave ->> +sinterface : HCI data
    Note over sinterface : Add Hosted header
    sinterface ->> -master : SPI/SDIO
    Note over sinterface,master : (VHCI data)
    Note over master : Remove Hosted header

    master ->> +vhci : hci_rx_handler()

    vhci ->> bt : notify_host_recv()
    Note over vhci, bt: HCI data

    vhci -->> -master : 
```

**Bluetooth Host Receiving Data**

### 5.2. Transporting HCI data using UART

When using BlueDroid Host Bluetooth Stack with UART, UART functions
that do the following are required:

- `uart_open` to open the UART driver and initialise the UART (set GPIOs, Baud Rate, etc.)
- `uart_tx` to transmit data over UART
- `UART RX` is a thread that waits for incoming UART data
- `notify_host_recv` is a BlueDroid callback registered with `UART RX` to receive UART data

`uart_open` is called before starting BlueDroid, while `uart_tx` and
`notify_host_recv` are registered by BlueDroid with the UART Driver. See this [ESP-IDF BlueDroid Example using UART](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/bluedroid_host_only/bluedroid_host_only_uart) for an example implementation.

#### 5.2.1. Bluetooth Host HCI Initialization

```mermaid
sequenceDiagram
    box Grey Master
    participant bt as Host Application
    participant huart as UART Driver
    end

    box Grey Co-processor
    participant slave as Bluetooth Controller with UART Interface
    end

    bt ->> +huart : uart_open()
	Note over huart: do any uart init required
    huart -->> -bt : 
```

**Bluetooth Host Initialization**

#### 5.2.2. Bluetooth Host Sending Data using HCI

```mermaid
sequenceDiagram
    box Grey Master
    participant bt as BlueDroid Host Bluetooth Stack
    participant huart as UART Driver
    end

    box Grey Co-processor
    participant slave as Bluetooth Controller with UART Interface
    end

    bt ->> huart : uart_tx()
    huart ->> slave : UART TX 
    Note over huart,slave : (standard HCI)
    huart -->> bt : 
```

**Bluetooth Host Sending Data**

#### 5.2.3. Bluetooth Host Receiving Data using HCI

```mermaid
sequenceDiagram
    box Grey Master
    participant bt as BlueDroid Host Bluetooth Stack
    participant huart as UART Driver
    end

    box Grey Co-processor
    participant slave as Bluetooth Controller with UART Interface
    end

    slave ->> huart : UART RX
    Note over slave,huart: (standard HCI)
    huart ->> bt : notify_host_recv()
    Note over huart, bt: HCI data

    bt -->> huart: 
```

**Bluetooth Host Receiving Data**

## 6. References

- esp-nimble: https://github.com/espressif/esp-nimble
- ESP-IDF NimBLE-based Host APIs: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/nimble/index.html
- Bluetooth API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/index.html
- ESP-IDF example using NimBLE on Host to send HCI through UART to co-processor: https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/bleprph_host_only
- ESP-IDF example using BlueDroid on Host to send HCI through UART to co-processor: https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/bluedroid_host_only/bluedroid_host_only_uart
