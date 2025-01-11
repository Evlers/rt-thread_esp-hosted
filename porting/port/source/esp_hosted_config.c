/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#include "esp_hosted_config.h"

#ifdef ESP_HOSTED_USING_PIN_NUMBER
rt_base_t esp_hosted_pin_reset = ESP_HOSTED_RESET_PIN;
rt_base_t esp_hosted_pin_cs = ESP_HOSTED_SPI_CS_PIN;
rt_base_t esp_hosted_pin_data_ready = ESP_HOSTED_DATA_READY_PIN;
rt_base_t esp_hosted_pin_handshake = ESP_HOSTED_HANDSHAKE_PIN;
#else
rt_base_t esp_hosted_pin_reset = -1;
rt_base_t esp_hosted_pin_cs = -1;
rt_base_t esp_hosted_pin_data_ready = -1;
rt_base_t esp_hosted_pin_handshake = -1;
#endif

static bool esp_hosted_config_set = false;

esp_err_t esp_hosted_set_default_config(void)
{
#ifdef ESP_HOSTED_USING_PIN_NAME
    esp_hosted_pin_reset = rt_pin_get(ESP_HOSTED_RESET_PIN_NAME);
    esp_hosted_pin_cs = rt_pin_get(ESP_HOSTED_SPI_CS_PIN_NAME);
    esp_hosted_pin_data_ready = rt_pin_get(ESP_HOSTED_DATA_READY_PIN_NAME);
    esp_hosted_pin_handshake = rt_pin_get(ESP_HOSTED_HANDSHAKE_PIN_NAME);
#endif
    esp_hosted_config_set = true;
    return ESP_OK;
}

bool esp_hosted_is_config_valid(void)
{
    return esp_hosted_config_set;
}
