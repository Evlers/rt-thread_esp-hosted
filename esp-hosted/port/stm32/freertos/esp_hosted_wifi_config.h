/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ESP_HOSTED_WIFI_CONFIG_H__
#define __ESP_HOSTED_WIFI_CONFIG_H__

#if CONFIG_SLAVE_CHIPSET_ESP32C5
// dual band API support available
#define H_WIFI_DUALBAND_SUPPORT 1
#else
#define H_WIFI_DUALBAND_SUPPORT 0
#endif

#endif
