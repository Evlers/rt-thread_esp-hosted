/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

/* APIs to do OTA updates of the co-processor */

#ifndef __ESP_HOSTED_OTA_H__
#define __ESP_HOSTED_OTA_H__

#include "esp_err.h"

esp_err_t esp_hosted_ota(const char* image_url);

#endif /*__ESP_HOSTED_OTA_H__*/
