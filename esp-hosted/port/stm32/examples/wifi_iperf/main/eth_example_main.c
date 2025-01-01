// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp_event.h"

#include "esp_log.h"
DEFINE_LOG_TAG(eth_ex);

#include "stm_ethernetif.h"

static void eth_event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data)
{
	if (event_base == IP_EVENT) {
		ESP_LOGI(TAG, "IP_EVENT, event_id %d", event_id);
		if (event_id == IP_EVENT_ETH_GOT_IP) {
			ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
			ESP_LOGI(TAG, "*** Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		} else if (event_id == IP_EVENT_ETH_LOST_IP) {
			ESP_LOGI(TAG, "*** Lost IP");
		}
	} else if (event_base == ETH_EVENT) {
		ESP_LOGI(TAG, "ETH_EVENT, event_id %d", event_id);
		switch (event_id)
		{
		case ETHERNET_EVENT_START:
			ESP_LOGI(TAG, "Eth Started");
			break;
		case ETHERNET_EVENT_STOP:
			ESP_LOGI(TAG, "Eth Stopped");
			break;
		case ETHERNET_EVENT_CONNECTED:
			ESP_LOGI(TAG, "Eth Connected");
			break;
		case ETHERNET_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "Eth Disconnected");
			break;
		default:
			ESP_LOGI(TAG, "UNKNOWN Eth Event");
			break;
		}
	}

}

static esp_netif_t *esp_netif = NULL;

void app_main(void)
{
	esp_event_handler_instance_t instance_got_ip;

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
			ESP_EVENT_ANY_ID,
			&eth_event_handler,
			NULL,
			&instance_got_ip));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(ETH_EVENT,
			ESP_EVENT_ANY_ID,
			&eth_event_handler,
			NULL,
			&instance_got_ip));

	esp_netif = stm_eth_if_init();
	assert(esp_netif);
}
