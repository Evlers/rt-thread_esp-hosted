/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
 * Adapted from https://github.com/espressif/esp-idf/blob/master/examples/wifi/getting_started/softAP/main/softap_example_main.c
 */


#include <string.h>
#include "os_wrapper.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "iperf.h"

/* iperf test config */
#define IPERF_TEST_IP_TYPE        IPERF_IP_TYPE_IPV4
#define IPERF_TEST_MODE           IPERF_FLAG_SERVER
#define IPERF_TEST_PROTOCOL       IPERF_FLAG_UDP
#define IPERF_TEST_DURATION_SEC   300
#define IPERF_TEST_INTERVAL_SEC   IPERF_DEFAULT_INTERVAL
#define IPERF_TEST_PKT_LEN        0 /* take default length from IPERF_UDP_TX_LEN/IPERF_UDP_RX_LEN or IPERF_TCP_TX_LEN/IPERF_TCP_RX_LEN */
#define IPERF_TEST_SRC_PORT       IPERF_DEFAULT_PORT
#define IPERF_TEST_DEST_PORT      IPERF_DEFAULT_PORT
#define IPERF_TEST_BW_LIMIT       IPERF_DEFAULT_NO_BW_LIMIT

#define EXAMPLE_ESP_WIFI_SSID      "ESP SoftAP"
#define EXAMPLE_ESP_WIFI_PASS      "ESPSoftAp"
#define EXAMPLE_ESP_WIFI_CHANNEL   6
#define EXAMPLE_MAX_STA_CONN       4

#if IPERF_TEST_MODE==RUN_IPERF_CLIENT
/* if your pc running iperf server and connected to same EXAMPLE_ESP_WIFI_SSID, provide its address */
#define IPERF_TEST_CLIENT_DEST_IP_STR "192.168.4.2"
#endif

#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

static const char *TAG = "wifi softAP";

/* FreeRTOS event group to signal when stations are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one :
 * - station connected to softAP
 */
#define STA_CONNECTED_BIT BIT(0)

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT) {
		if (event_id == WIFI_EVENT_AP_STACONNECTED) {
			wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
			ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
					MAC2STR(event->mac), event->aid);
		} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
			wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
			ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
					MAC2STR(event->mac), event->aid, event->reason);
		}
	} else if (event_base == IP_EVENT) {
		if (event_id == IP_EVENT_AP_STAIPASSIGNED) {
			// a station got assigned an IP address: iperf can now start
			xEventGroupSetBits(s_wifi_event_group, STA_CONNECTED_BIT);
		}
	}

}

static esp_netif_t *netif_softap = NULL;

void wifi_init_softap(void)
{
	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());

	netif_softap = esp_netif_create_default_wifi_ap();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	cfg.nvs_enable = H_ESP_WIFI_NVS_ENABLED; // this determines if WiFi config is saved in NVS on co-processor
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
			ESP_EVENT_ANY_ID,
			&wifi_event_handler,
			NULL,
			NULL));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
			ESP_EVENT_ANY_ID,
			&wifi_event_handler,
			NULL,
			NULL));

	wifi_config_t wifi_config = {
			.ap = {
					.ssid = EXAMPLE_ESP_WIFI_SSID,
					.ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
					.channel = EXAMPLE_ESP_WIFI_CHANNEL,
					.password = EXAMPLE_ESP_WIFI_PASS,
					.max_connection = EXAMPLE_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
					.authmode = WIFI_AUTH_WPA3_PSK,
					.sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
					.authmode = WIFI_AUTH_WPA2_PSK,
#endif
					.pmf_cfg = {
							.required = true,
					},
			},
	};
	if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
			EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

static uint32_t wifi_get_local_ip(void)
{
	esp_netif_t *netif = netif_softap;
	esp_netif_ip_info_t ip_info = {0};

	esp_netif_get_ip_info(netif, &ip_info);

	return ip_info.ip.addr;
}

// number of seconds before connecting to a iperf server
#define IPERF_CLIENT_WAIT_SECS 5

void run_iperf(void)
{
	iperf_cfg_t cfg = {0};

#if IPERF_TEST_MODE==RUN_IPERF_CLIENT
	cfg.flag |= IPERF_FLAG_CLIENT;
	cfg.destination_ip4 = esp_ip4addr_aton(IPERF_TEST_CLIENT_DEST_IP_STR);
#endif

	cfg.type = IPERF_TEST_IP_TYPE;

	/* iperf -c? */
	cfg.flag |= IPERF_TEST_MODE;

	/* iperf -B */
	cfg.source_ip4 = wifi_get_local_ip();
	if (cfg.source_ip4 == 0) {
		ESP_LOGE(TAG, "ERROR geting source ip");
		return;
	}

	/* iperf -u? */
	cfg.flag |= IPERF_TEST_PROTOCOL;


	/* iperf -l */
	cfg.len_send_buf = IPERF_TEST_PKT_LEN;

	/* source, destination ports iperf -p */
	cfg.sport = IPERF_TEST_SRC_PORT;
	cfg.dport = IPERF_TEST_DEST_PORT;

	/* iperf -i */
	cfg.interval = IPERF_TEST_INTERVAL_SEC;

	/* iperf -t */
	cfg.time = IPERF_TEST_DURATION_SEC;

	/* iperf -b */
	cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;

	ESP_LOGI(TAG, "mode=%s-%s sip=%d.%d.%d.%d:%d, dip=%d.%d.%d.%d:%d, interval=%d, time=%d",
			 cfg.flag & IPERF_FLAG_TCP ? "tcp" : "udp",
			 cfg.flag & IPERF_FLAG_SERVER ? "server" : "client",
			 cfg.source_ip4 & 0xFF, (cfg.source_ip4 >> 8) & 0xFF, (cfg.source_ip4 >> 16) & 0xFF,
			 (cfg.source_ip4 >> 24) & 0xFF, cfg.sport,
			 cfg.destination_ip4 & 0xFF, (cfg.destination_ip4 >> 8) & 0xFF,
			 (cfg.destination_ip4 >> 16) & 0xFF, (cfg.destination_ip4 >> 24) & 0xFF, cfg.dport,
			 cfg.interval, cfg.time);

#if IPERF_TEST_MODE==RUN_IPERF_CLIENT
	ESP_LOGI(TAG, "wait seconds before connecting to iperf server...");
	g_h.funcs->_h_sleep(IPERF_CLIENT_WAIT_SECS);
#endif

	iperf_start(&cfg);
}

void app_main(void)
{
	s_wifi_event_group = xEventGroupCreate();

	wifi_init_softap();

	// wait until a station is connected before starting
	xEventGroupWaitBits(s_wifi_event_group,
			STA_CONNECTED_BIT,
			pdFALSE,
			pdFALSE,
			portMAX_DELAY);

	ESP_LOGI(TAG, "Running iperf");

	run_iperf();
}
