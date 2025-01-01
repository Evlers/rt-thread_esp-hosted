/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

/* APIs to do OTA updates of the co-processor */

/** prevent recursive inclusion **/
#ifndef __ESP_HOSTED_OTA_H__
#define __ESP_HOSTED_OTA_H__


#ifdef __cplusplus
extern "C" {
#endif

#include "lwip/ip_addr.h"

typedef struct {
	ip_addr_t ip_addr;
	uint16_t port;
	char * filepath;
} esp_hosted_ota_http_t;

esp_err_t esp_hosted_ota_http(const esp_hosted_ota_http_t* url);

#ifdef __cplusplus
}
#endif

#endif
