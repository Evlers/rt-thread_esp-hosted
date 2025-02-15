/*
 * Copyright (c) 2025 Evlers
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Change Logs:
 * Date         Author      Notes
 * 2025-01-01   Evlers      first implementation
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
