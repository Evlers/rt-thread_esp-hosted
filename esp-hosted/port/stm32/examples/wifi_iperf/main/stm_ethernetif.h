/*
 * SPDX-FileCopyrightText: 2019-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PORT_STM32_EXAMPLES_WIFI_IPERF_MAIN_STM_ETHERNETIF_H_
#define PORT_STM32_EXAMPLES_WIFI_IPERF_MAIN_STM_ETHERNETIF_H_

#include "esp_netif.h"

esp_netif_t * stm_eth_if_init(void);

/**
* @brief Ethernet event declarations
*
*/
typedef enum {
	ETHERNET_EVENT_START,        /*!< Ethernet driver start */
	ETHERNET_EVENT_STOP,         /*!< Ethernet driver stop */
	ETHERNET_EVENT_CONNECTED,    /*!< Ethernet got a valid link */
	ETHERNET_EVENT_DISCONNECTED, /*!< Ethernet lost a valid link */
} eth_event_t;

/**
* @brief Ethernet event base declaration
*
*/
ESP_EVENT_DECLARE_BASE(ETH_EVENT);

#endif /* PORT_STM32_EXAMPLES_WIFI_IPERF_MAIN_STM_ETHERNETIF_H_ */
