// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/** Includes **/
#include "transport_drv.h"
#include "common/stats.h"

#define DBG_TAG           "esp.trans"
#define DBG_LVL           DBG_INFO
#include "rtdbg.h"


/**
 * @brief  open virtual network device
 * @param  netdev - network device
 * @retval 0 on success
 */
int esp_netdev_open(netdev_handle_t netdev)
{
	return STM_OK;
}

/**
 * @brief  close virtual network device
 * @param  netdev - network device
 * @retval 0 on success
 */
int esp_netdev_close(netdev_handle_t netdev)
{
	return STM_OK;
}


/**
 * @brief  transmit on virtual network device
 * @param  netdev - network device
 *         net_buf - buffer to transmit
 * @retval STM_OK for success or failure from enum stm_ret_t
 */
int esp_netdev_xmit(netdev_handle_t netdev, struct pbuf *net_buf)
{
	struct esp_private *priv = NULL;
	int ret = 0;

	if (!netdev || !net_buf)
		return STM_FAIL;
	priv = (struct esp_private *) netdev_stub_get_priv(netdev);

	if (!priv)
		return STM_FAIL;

	ret = send_to_slave(priv->if_type, priv->if_num,
			net_buf->payload, net_buf->len);

	free(net_buf);

	return ret;
}

void process_capabilities(uint8_t cap)
{
#if DEBUG_TRANSPORT
	LOG_D("capabilities: 0x%x",cap);
#else
	/* warning suppress */
	if(cap);
#endif
}

void process_priv_communication(struct pbuf *pbuf)
{
	struct esp_priv_event *header = NULL;

	uint8_t *payload = NULL;
	uint16_t len = 0;

	if (!pbuf || !pbuf->payload)
		return;

	header = (struct esp_priv_event *) pbuf->payload;

	payload = pbuf->payload;
	len = pbuf->len;

	if (header->event_type == ESP_PRIV_EVENT_INIT)
	{
		LOG_D("event packet type");
		process_event(payload, len);
	}

	hosted_free(pbuf);
}

static void print_capabilities(uint32_t cap)
{
	LOG_I("Features supported are:");
	if (cap & (ESP_WLAN_SDIO_SUPPORT | ESP_WLAN_SPI_SUPPORT))
		LOG_I("\t * WLAN");
	if (cap & (ESP_BT_UART_SUPPORT | ESP_BT_SDIO_SUPPORT | ESP_BT_SPI_SUPPORT))
	{
		LOG_I("\t * BT/BLE");
		if (cap & ESP_BT_UART_SUPPORT)
			LOG_I("\t   - HCI over UART");
		if (cap & ESP_BT_SDIO_SUPPORT)
			LOG_I("\t   - HCI over SDIO");
		if (cap & ESP_BT_SPI_SUPPORT)
			LOG_I("\t   - HCI over SPI");
		if ((cap & ESP_BLE_ONLY_SUPPORT) && (cap & ESP_BR_EDR_ONLY_SUPPORT))
			LOG_I("\t   - BT/BLE dual mode");
		else if (cap & ESP_BLE_ONLY_SUPPORT)
			LOG_I("\t   - BLE only");
		else if (cap & ESP_BR_EDR_ONLY_SUPPORT)
			LOG_I("\t   - BR EDR only");
	}
}

static void print_chip_type(char chip_type)
{
	if ((chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32) &&
	    (chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32S2) &&
	    (chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32C2) &&
	    (chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32C3) &&
	    (chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32C6) &&
	    (chip_type != ESP_PRIV_FIRMWARE_CHIP_ESP32S3)) 
	{
		LOG_E("ESP board type is not mentioned, ignoring [%d]", chip_type);
		chip_type = ESP_PRIV_FIRMWARE_CHIP_UNRECOGNIZED;
	}
	else
	{
		LOG_I("Chip is: %s%s%s%s%s%s",
				chip_type == ESP_PRIV_FIRMWARE_CHIP_ESP32 ? "ESP32" : "",
				chip_type == ESP_PRIV_FIRMWARE_CHIP_ESP32S2 ? "ESP32s2" : "",
				chip_type == ESP_PRIV_FIRMWARE_CHIP_ESP32C2 ? "ESP32c2" : "",
				chip_type == ESP_PRIV_FIRMWARE_CHIP_ESP32C3 ? "ESP32c3" : "",
				chip_type == ESP_PRIV_FIRMWARE_CHIP_ESP32C6 ? "ESP32c6" : "",
				chip_type == ESP_PRIV_FIRMWARE_CHIP_ESP32S3 ? "ESP32s3" : "");
	}
}

void process_event(uint8_t *evt_buf, uint16_t len)
{
	struct esp_priv_event *event;

	if (!evt_buf || !len)
		return;

	event = (struct esp_priv_event *) evt_buf;

	if (event->event_type == ESP_PRIV_EVENT_INIT)
	{
		LOG_D("Received INIT event from ESP peripheral");

		print_hex_dump(event->event_data, event->event_len, "process event");

		if (process_init_event(event->event_data, event->event_len))
		{
			LOG_E("failed to init event");
		}
	}
	else
	{
		LOG_W("Drop unknown event");
	}
}

static void adjust_spi_clock(uint8_t spi_clk_mhz)
{
	// if ((spi_clk_mhz) && (spi_clk_mhz != SPI_INITIAL_CLK_MHZ)) {
	// 	printk(KERN_INFO "ESP Reconfigure SPI CLK to %u MHz\n",spi_clk_mhz);
	// 	spi_context.spi_clk_mhz = spi_clk_mhz;
	// 	spi_context.esp_spi_dev->max_speed_hz = spi_clk_mhz * NUMBER_1M;
	// }
}

int process_init_event(uint8_t *evt_buf, uint8_t len)
{
	uint8_t len_left = len, tag_len;
	uint8_t *pos;

	if (!evt_buf)
		return STM_FAIL;

	pos = evt_buf;
	while (len_left)
	{
		tag_len = *(pos + 1);
		LOG_D("EVENT: %d", *pos);
		if (*pos == ESP_PRIV_CAPABILITY)
		{
			LOG_D("priv capabilty ");
			process_capabilities(*(pos + 2));
			print_capabilities(*(pos + 2));
		}
		else if (*pos == ESP_PRIV_SPI_CLK_MHZ)
		{
			LOG_D("adjust spi clock: %uMHz", (*(pos + 2)));
			adjust_spi_clock(*(pos + 2));
		}
		else if (*pos == ESP_PRIV_FIRMWARE_CHIP_ID)
		{
			print_chip_type(*(pos+2));
		}
		else if (*pos == ESP_PRIV_TEST_RAW_TP)
		{
			LOG_D("priv test raw tp");
#if TEST_RAW_TP
			process_test_capabilities(*(pos + 2));
#endif
		}
		else
		{
			LOG_W("Unsupported tag in event");
		}
		pos += (tag_len+2);
		len_left -= (tag_len+2);
	}

	return STM_OK;
}
