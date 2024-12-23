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

void(*transport_esp_hosted_up_cb)(void) = NULL;

/**
 * @brief  open virtual network device
 * @param  netdev - network device
 * @retval 0 on success
 */
int esp_netdev_open(netdev_handle_t netdev)
{
    return ESP_OK;
}

/**
 * @brief  close virtual network device
 * @param  netdev - network device
 * @retval 0 on success
 */
int esp_netdev_close(netdev_handle_t netdev)
{
    return ESP_OK;
}


/**
 * @brief  transmit on virtual network device
 * @param  netdev - network device
 *         net_buf - buffer to transmit
 * @retval ESP_OK for success or failure from enum esp_ret
 */
int esp_netdev_xmit(netdev_handle_t netdev, struct pbuf *net_buf)
{
    struct esp_private *priv = NULL;

    if (!netdev || !net_buf)
        return ESP_FAIL;
    priv = (struct esp_private *) netdev_stub_get_priv(netdev);

    if (!priv)
        return ESP_FAIL;

    if (send_to_slave(priv->if_type, priv->if_num, net_buf->payload, net_buf->len) != ESP_OK)
        return ESP_FAIL;

    free(net_buf);

    return ESP_OK;
}

static void transport_driver_event_handler(uint8_t event)
{
	switch(event)
	{
		case TRANSPORT_ACTIVE:
		{
			/* Initiate control path now */
			LOG_D("Base transport is set-up");
			if (transport_esp_hosted_up_cb)
				transport_esp_hosted_up_cb();
			break;
		}

		default:
		break;
	}
}

void transport_drv_init(void(*esp_hosted_up_cb)(void))
{
	transport_init_internal();
	transport_esp_hosted_up_cb = esp_hosted_up_cb;
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

void process_priv_communication(interface_buffer_handle_t *buf_handle)
{
	if (!buf_handle || !buf_handle->payload || !buf_handle->payload_len)
		return;

	process_event(buf_handle->payload, buf_handle->payload_len);
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

int process_init_event(uint8_t *evt_buf, uint8_t len)
{
    uint8_t len_left = len, tag_len;
    uint8_t *pos;

    if (!evt_buf)
        return ESP_FAIL;

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
        else if (*pos == ESP_PRIV_FW_DATA)
        {
            struct fw_version *fw_ver = (struct fw_version *) (pos + 2);
            LOG_D("ESP-Hosted Firmware version :: %s-%d.%d.%d.%d.%d",
                    fw_ver->project_name, fw_ver->major1, fw_ver->major2, fw_ver->minor,
                    fw_ver->revision_patch_1, fw_ver->revision_patch_2);
        }
        else
        {
            LOG_W("Unsupported tag in event: %d", *pos);
        }
        pos += (tag_len+2);
        len_left -= (tag_len+2);
    }

    transport_driver_event_handler(TRANSPORT_ACTIVE);
    return ESP_OK;
}
