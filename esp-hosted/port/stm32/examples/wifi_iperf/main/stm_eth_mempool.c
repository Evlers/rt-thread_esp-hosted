/*
 * SPDX-FileCopyrightText: 2019-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Simple mempool implementation
 *
 * Statically allocated in a named section that the linker will put into memory accessible by the Ethernet driver
 */

#include <string.h>

#include "stm_eth_mempool.h"

#include "esp_log.h"
static const char *TAG = "eth-mempool";

typedef struct {
	ETH_AppBuff buf;
	bool in_use;
} stm_eth_rx_buf_t;

/* Create our own memory pool implementation for Rx */

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma location = 0x30000100
static stm_eth_rx_buf_t stm_eth_rx_pool[ETH_RX_BUFFER_CNT];

#elif defined ( __CC_ARM )  /* MDK ARM Compiler */
__attribute__((section(".Rx_PoolSection"))) static stm_eth_rx_buf_t stm_eth_rx_pool[ETH_RX_BUFFER_CNT];

#elif defined ( __GNUC__ ) /* GNU Compiler */
__attribute__((section(".Rx_PoolSection"))) static stm_eth_rx_buf_t stm_eth_rx_pool[ETH_RX_BUFFER_CNT];

#endif

static bool inited = false;

void stm_rx_mempool_init(void)
{
	int i;

	ESP_LOGD(TAG, "mempool loc %p", stm_eth_rx_pool);

	if (inited) {
		return;
	}

	for (i = 0; i < ETH_RX_BUFFER_CNT; i++) {
		stm_eth_rx_pool[i].in_use = false;
		memset(&stm_eth_rx_pool[i].buf, 0, sizeof(ETH_AppBuff));
	}
	inited = true;
}

void stm_rx_mempool_deinit(void)
{
	inited = false;

	// do nothing as mempool is a static allocation
}

ETH_AppBuff * stm_rx_mempool_get(void)
{
	int i;

	for (i = 0; i < ETH_RX_BUFFER_CNT; i++) {
		if (!stm_eth_rx_pool[i].in_use) {
			stm_eth_rx_pool[i].in_use = true;
			return &stm_eth_rx_pool[i].buf;
		}
	}

	// no free memory
	ESP_LOGE(TAG, "out of mempool memory");
	return NULL;
}

void stm_rx_mempool_release(ETH_AppBuff * ptr)
{
	int i;

	if (!ptr) {
		ESP_LOGE(TAG, "got NULL mempool pointer");
		return;
	}

	for (i = 0; i < ETH_RX_BUFFER_CNT; i++) {
		if (ptr == &stm_eth_rx_pool[i].buf) {
			if (stm_eth_rx_pool[i].in_use) {
				stm_eth_rx_pool[i].in_use = false;
				return;
			} else {
				ESP_LOGE(TAG, "1-mempool memory was not in use");
				return;
			}
		}
	}
	ESP_LOGE(TAG, "1-invalid mempool memory to be released");
}

// this releases the memory based on the provided buffer ref
void stm_rx_mempool_release_buf_ref(void * ptr)
{
	int i;

	if (!ptr) {
		ESP_LOGE(TAG, "got NULL buf pointer");
		return;
	}

	for (i = 0; i < ETH_RX_BUFFER_CNT; i++) {
		if (ptr == &stm_eth_rx_pool[i].buf.buffer) {
			if (stm_eth_rx_pool[i].in_use) {
				stm_eth_rx_pool[i].in_use = false;
				return;
			} else {
				ESP_LOGE(TAG, "2-mempool memory was not in use");
				return;
			}
		}
	}
	ESP_LOGE(TAG, "2-invalid mempool memory to be released");
}
