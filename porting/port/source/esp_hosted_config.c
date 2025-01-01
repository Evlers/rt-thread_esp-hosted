/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#include "esp_hosted_config.h"

static bool esp_hosted_config_set = false;

esp_err_t esp_hosted_set_default_config(void)
{
    esp_hosted_config_set = true;
    return ESP_OK;
}

bool esp_hosted_is_config_valid(void)
{
    return esp_hosted_config_set;
}
