/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

/* Current OTA method(s) supported:
 * - OTA from a HTTP URL
 *
 * Procedure:
 * 1. Prepare OTA binary
 * 2. Call rpc_ota_begin() to start OTA
 * 3. Repeatedly call rpc_ota_write() with a continuous chunk of OTA data
 * 4. Call rpc_ota_end()
 */

#include "esp_log.h"

#include "esp_hosted_ota.h"
#include "rpc_wrap.h"

#include "lwip/apps/http_client.h"

typedef enum {
	OTA_STATE_IDLE,
	OTA_STATE_STARTED,
} ota_state_t;

DEFINE_LOG_TAG(ota);

#define OTA_TIMEOUT_SEC (2 * 60)
static int ota_state = OTA_STATE_IDLE;
void * ota_sem = NULL;

/*** Start OTA via a HTTP URL ***/

/* result_fn is triggered at the end of the http request.
 * httpc_result contains the result (OK, HTTP TIMEOUT, etc.),
 * but here we only trigger the semaphore to end the OTA process.
 */
static void result_fn(void *arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err)
{
	ESP_LOGV(TAG, "http request done: httpc_result = %d, rx_content_len = %ld, srv_res = %ld, err = %d",
			httpc_result, rx_content_len, srv_res, err);

	g_h.funcs->_h_post_semaphore(ota_sem);
}

/* headers_done_fn is triggered when we get the HTTP header
 * we use it to signal the start of the OTA process
 */
static err_t headers_done_fn(httpc_state_t *connection, void *arg, struct pbuf *hdr, u16_t hdr_len, u32_t content_len)
{
	int ret;

	ESP_LOGV(TAG, "Got Headers");
	// begin ota
	ota_state = OTA_STATE_STARTED;

	ret = rpc_ota_begin();
	if (ret != SUCCESS) {
		ESP_LOGE(TAG, "rpc_ota_begin ERROR");
		return ERR_ABRT;
	}
	return ERR_OK;
}

static int total_len = 0;

/* recv_fn is triggered when we get data */
err_t recv_fn(void *arg, struct altcp_pcb *conn, struct pbuf *p, err_t err)
{
	struct pbuf *next;
	err_t result = ERR_OK;
	esp_err_t res;

	if (p && (ota_state == OTA_STATE_STARTED)) {
		total_len += p->len;
		ESP_LOGV(TAG, "got data: len %d, total_len %d", p->len, total_len);

		// write_ota
		res = rpc_ota_write(p->payload, p->len);
		if (res) {
			ESP_LOGE(TAG, "rpc_ota_write ERROR");
			result = ERR_ABRT;
			goto recv_fn_error;
		}

		next = p->next;
		while (next) {
			total_len += next->len;
			ESP_LOGV(TAG, "got more data: len %d, total_len %d", next->len, total_len);

			// write_ota
			res = rpc_ota_write(next->payload, next->len);
			if (res) {
				ESP_LOGE(TAG, "rpc_ota_write ERROR");
				result = ERR_ABRT;
				goto recv_fn_error;
			}
			next = next->next;
		}
	}

recv_fn_error:
	if (p) {
		/* see the else branch of httpc_tcp_recv() for state HTTPC_PARSE_RX_DATA
		 * to see what the recv function should do when it has processed valid data
		 */
		// acknowledge the data we have received
		altcp_recved(conn, p->tot_len);
		// free the pbuf
		pbuf_free(p);
	}
	return result;
}

// cannot be stack variable: settings is referenced in http_client.c, not copied
static httpc_connection_t settings = { 0 };

/* OTA process
 * we wait on a semaphore to indicate the end of download,
 * then check whether the OTA succeeded or not
 */
esp_err_t esp_hosted_ota_http(const esp_hosted_ota_http_t* ota_url)
{
	err_t res;

	ota_sem = g_h.funcs->_h_create_semaphore(1);
	if (!ota_sem) {
		ESP_LOGE(TAG, "error creating semaphore");
		return ESP_FAIL;
	}
	g_h.funcs->_h_get_semaphore(ota_sem, portMAX_DELAY);

	total_len = 0;
	ota_state = OTA_STATE_IDLE;

	settings.result_fn = result_fn;
	settings.headers_done_fn = headers_done_fn;

	res = httpc_get_file(&ota_url->ip_addr, ota_url->port, ota_url->filepath,
			&settings, recv_fn,
			NULL, NULL);

	if (res != ERR_OK) {
		ESP_LOGE(TAG, "http request failed");
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "waiting for download to finish");
	g_h.funcs->_h_get_semaphore(ota_sem, OTA_TIMEOUT_SEC);
	ESP_LOGI(TAG, "download DONE");

	g_h.funcs->_h_destroy_semaphore(ota_sem);
	ota_sem = NULL;

	if (ota_state != OTA_STATE_IDLE) {
		// ota was started (not idle): end ota
		if (rpc_ota_end() != SUCCESS) {
			ESP_LOGE(TAG, "FAILED");
			return ESP_FAIL;
		}
		ESP_LOGV(TAG, "success");
		return ESP_OK;
	} else {
		ESP_LOGW(TAG, "never started");
	}
	return ESP_FAIL;
}

/*** End OTA via a HTTP URL ***/
