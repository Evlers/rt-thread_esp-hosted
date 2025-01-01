/* Scan Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
    This example shows how to scan for available set of APs.

	Adapted from on https://github.com/espressif/esp-idf/tree/master/examples/wifi/scan
*/

#include <string.h>
#include "os_wrapper.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"

#define DEFAULT_SCAN_LIST_SIZE 10

static const char *TAG = "scan";

static void print_auth_mode(int authmode)
{
	switch (authmode) {
	case WIFI_AUTH_OPEN:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
		break;
	case WIFI_AUTH_OWE:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OWE");
		break;
	case WIFI_AUTH_WEP:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
		break;
	case WIFI_AUTH_WPA_PSK:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
		break;
	case WIFI_AUTH_WPA2_PSK:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
		break;
	case WIFI_AUTH_WPA_WPA2_PSK:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
		break;
	case WIFI_AUTH_ENTERPRISE:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_ENTERPRISE");
		break;
	case WIFI_AUTH_WPA3_PSK:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
		break;
	case WIFI_AUTH_WPA2_WPA3_PSK:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
		break;
	case WIFI_AUTH_WPA3_ENTERPRISE:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENTERPRISE");
		break;
	case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_ENTERPRISE");
		break;
	case WIFI_AUTH_WPA3_ENT_192:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENT_192");
		break;
	default:
		ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
		break;
	}
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
	switch (pairwise_cipher) {
	case WIFI_CIPHER_TYPE_NONE:
		ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
		break;
	case WIFI_CIPHER_TYPE_WEP40:
		ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
		break;
	case WIFI_CIPHER_TYPE_WEP104:
		ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
		break;
	case WIFI_CIPHER_TYPE_TKIP:
		ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
		break;
	case WIFI_CIPHER_TYPE_CCMP:
		ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
		break;
	case WIFI_CIPHER_TYPE_TKIP_CCMP:
		ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
		break;
	case WIFI_CIPHER_TYPE_AES_CMAC128:
		ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_AES_CMAC128");
		break;
	case WIFI_CIPHER_TYPE_SMS4:
		ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_SMS4");
		break;
	case WIFI_CIPHER_TYPE_GCMP:
		ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP");
		break;
	case WIFI_CIPHER_TYPE_GCMP256:
		ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP256");
		break;
	default:
		ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
		break;
	}

	switch (group_cipher) {
	case WIFI_CIPHER_TYPE_NONE:
		ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
		break;
	case WIFI_CIPHER_TYPE_WEP40:
		ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
		break;
	case WIFI_CIPHER_TYPE_WEP104:
		ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
		break;
	case WIFI_CIPHER_TYPE_TKIP:
		ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
		break;
	case WIFI_CIPHER_TYPE_CCMP:
		ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
		break;
	case WIFI_CIPHER_TYPE_TKIP_CCMP:
		ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
		break;
	case WIFI_CIPHER_TYPE_SMS4:
		ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_SMS4");
		break;
	case WIFI_CIPHER_TYPE_GCMP:
		ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP");
		break;
	case WIFI_CIPHER_TYPE_GCMP256:
		ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP256");
		break;
	default:
		ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
		break;
	}
}

/* FreeRTOS event group to signal events*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event:
 * - wifi started
 */
#define WIFI_STARTED_BIT BIT(0)

static void event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT) {
		ESP_LOGV(TAG, "Event: %d", event_id);
		if (event_id == WIFI_EVENT_STA_START) {
			ESP_LOGI(TAG, "Event: Station Started");
			xEventGroupSetBits(s_wifi_event_group, WIFI_STARTED_BIT);
		}
	}
}

static void wifi_scan(void)
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	cfg.nvs_enable = H_ESP_WIFI_NVS_ENABLED; // this determines if WiFi config is saved in NVS on co-processor
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
			ESP_EVENT_ANY_ID,
			&event_handler,
			NULL,
			&instance_any_id));

	uint16_t number = DEFAULT_SCAN_LIST_SIZE;
	wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
	uint16_t ap_count = 0;
	memset(ap_info, 0, sizeof(ap_info));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());

	/* Wait until wifi has been started */
	xEventGroupWaitBits(s_wifi_event_group,
			WIFI_STARTED_BIT,
			pdFALSE,
			pdFALSE,
			portMAX_DELAY);

	// do the scan
	ESP_LOGI(TAG, "scanning...");
	esp_wifi_scan_start(NULL, true);

	ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
	ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
	for (int i = 0; i < number; i++) {
		ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
		ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
		print_auth_mode(ap_info[i].authmode);
		if (ap_info[i].authmode != WIFI_AUTH_WEP) {
			print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
		}
		ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
		ESP_LOGI(TAG, "");
	}
}

void app_main(void)
{
	wifi_scan();
}
