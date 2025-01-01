
// Copyright 2015-2024 Espressif Systems (Shanghai) PTE LTD
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

/* This example
 * - gets the co-procesor version info
 * - displays the BSSID when connected to the AP
 * - periodically gets the RSSI and BSSID of the connected AP
 */

#include <string.h>
#include "os_wrapper.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_hosted_api.h"

#ifndef CONFIG_ESP_WIFI_SSID
#define CONFIG_ESP_WIFI_SSID "SaveEarth"
#endif

#ifndef CONFIG_ESP_WIFI_PASSWORD
#define CONFIG_ESP_WIFI_PASSWORD "PlantMoreTrees123"
#endif

#ifndef CONFIG_ESP_MAXIMUM_RETRY
#define CONFIG_ESP_MAXIMUM_RETRY 5
#endif

#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#else
/* default */
#define ESP_WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT(0)
#define WIFI_FAIL_BIT      BIT(1)

#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

DEFINE_LOG_TAG(rssi_ex);

static int s_retry_num = 0;

static esp_netif_t *netif_sta = NULL;

static void event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT) {
		if (event_id == WIFI_EVENT_STA_START) {
			ESP_LOGI(TAG, "Connect to the AP[%s]", EXAMPLE_ESP_WIFI_SSID);
			esp_wifi_connect();
		} else if (event_id == WIFI_EVENT_STA_CONNECTED) {
			ESP_LOGI(TAG, "Connected to the AP[%s]", EXAMPLE_ESP_WIFI_SSID);
			wifi_event_sta_connected_t * ap_info = (wifi_event_sta_connected_t *)event_data;
			ESP_LOGI(TAG, "BSSID of AP: " MACSTR, MAC2STR(ap_info->bssid));
		} else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
			if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
				esp_wifi_connect();
				s_retry_num++;
				ESP_LOGI(TAG, "retry[%u] to connect to the AP[%s]",
						s_retry_num, EXAMPLE_ESP_WIFI_SSID);
			} else {
				xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
			}
			ESP_LOGW(TAG, "Retries exhausted, Failed to connect AP[%s] fail", CONFIG_ESP_WIFI_SSID);
		}
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		printf("Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}

}

int wifi_init_sta(void)
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	netif_sta = esp_netif_create_default_wifi_sta();
	assert(netif_sta);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	cfg.nvs_enable = H_ESP_WIFI_NVS_ENABLED; // this determines if WiFi config is saved in NVS on co-processor

	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;

	// get fw version
	esp_hosted_coprocessor_fwver_t ver_info;
	esp_hosted_get_coprocessor_fwversion(&ver_info);
	ESP_LOGI(TAG, "ESP Hosted Co-processor FW Version: %ld.%ld.%ld",
			ver_info.major1, ver_info.minor1, ver_info.patch1);

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
			ESP_EVENT_ANY_ID,
			&event_handler,
			NULL,
			&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
			IP_EVENT_STA_GOT_IP,
			&event_handler,
			NULL,
			&instance_got_ip));

	wifi_config_t wifi_config = {
			.sta = {
					.ssid = EXAMPLE_ESP_WIFI_SSID,
					.password = EXAMPLE_ESP_WIFI_PASS,
					/* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
					 * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
					 * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
					 * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
					 */
					.threshold.authmode = ESP_WIFI_AUTH_MODE_THRESHOLD,
					.sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
					/* Scan all channels and connect by signal strength
					 * to ensure we connect to the SSID with the strongest signal strength
					 * if the SSID is present on multiple channels
					 */
					.scan_method = WIFI_ALL_CHANNEL_SCAN,
					.sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
			},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
			WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
			pdFALSE,
			pdFALSE,
			portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s", EXAMPLE_ESP_WIFI_SSID);
		return 0;
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s", EXAMPLE_ESP_WIFI_SSID);
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}
	return -1;
}

#define DELAY_SEC 2
static int wifi_cmd_rssi_bssid(void)
{
	esp_err_t res;

	int rssi;
	wifi_config_t wifi_config;

	while (1) {
		res = esp_wifi_sta_get_rssi(&rssi);
		if (res) {
			ESP_LOGE(TAG, "failed to get wifi config");
		} else {
			ESP_LOGE(TAG, "rssi: %d", rssi);
		}
        g_h.funcs->_h_sleep(DELAY_SEC);
	}
}

void app_main(void)
{
	if (0 == wifi_init_sta()) {
		// only get RSSI and BSSID when we are connected
		wifi_cmd_rssi_bssid();
	}
}
