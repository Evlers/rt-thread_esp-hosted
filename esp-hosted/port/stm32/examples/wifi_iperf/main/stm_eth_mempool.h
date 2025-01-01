/*
 * SPDX-FileCopyrightText: 2019-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PORT_STM32_EXAMPLES_WIFI_IPERF_MAIN_STM_ETH_MEMPOOL_H_
#define PORT_STM32_EXAMPLES_WIFI_IPERF_MAIN_STM_ETH_MEMPOOL_H_

#include <stdbool.h>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_eth.h"

// #define ETH_RX_BUFFER_SIZE            1000U
// #define ETH_RX_BUFFER_SIZE            1518U
#define ETH_RX_BUFFER_SIZE            1536U
#define ETH_RX_BUFFER_CNT             12U
#define ETH_TX_BUFFER_MAX             ((ETH_TX_DESC_CNT) * 2U)

#define ETHIF_TX_TIMEOUT              (2000U)

typedef struct {
	ETH_BufferTypeDef AppBuff;
	uint8_t buffer[(ETH_RX_BUFFER_SIZE + 31) & ~31] __ALIGNED(32);
} ETH_AppBuff;

/* Initialises the mempool */
void stm_rx_mempool_init(void);

/* Deinit the mempool */
void stm_rx_mempool_deinit(void);

/* Get a pointer to a ETH_AppBuff from the mempool */
ETH_AppBuff * stm_rx_mempool_get(void);

/* release the mempool allocated ETH_AppBuff */
void stm_rx_mempool_release(ETH_AppBuff * ptr);

/* release the mempool allocated ETH_AppBuff, using the buffer address as the reference */
void stm_rx_mempool_release_buf_ref(void * ptr);

#endif /* PORT_STM32_EXAMPLES_WIFI_IPERF_MAIN_STM_ETH_MEMPOOL_H_ */
