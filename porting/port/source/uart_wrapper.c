// SPDX-License-Identifier: Apache-2.0
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


#include "esp_hosted_config.h"
#include "esp_hosted_log.h"
#include "os_wrapper.h"
#include "uart_wrapper.h"


// #if H_UART_START_BITS != 1
// #error "UART Start Bits must be 1 to communicate with ESP co-processor"
// #endif

#if H_UART_FLOWCTRL
#error "UART Flow Control must be disabled to communicate with ESP co-processor"
#endif

#if CONFIG_ESP_CONSOLE_UART
#if CONFIG_ESP_CONSOLE_UART_NUM == H_UART_PORT
#error "ESP Console UART and Hosted UART are the same. Select another UART port."
#endif
#endif

// these values should match ESP_UART_PARITY values in Hosted Kconfig
enum {
	HOSTED_UART_PARITY_NONE = 0,
	HOSTED_UART_PARITY_EVEN = 1,
	HOSTED_UART_PARITY_ODD = 2,
};

// these values should match ESP_UART_STOP_BITS values in Hosted Kconfig
enum {
	HOSTED_STOP_BITS_1 = 0,
	HOSTED_STOP_BITS_1_5 = 1,
	HOSTED_STOP_BITS_2 = 2,
};


int hosted_wait_rx_data(uint32_t ticks_to_wait)
{
	return 0;
}

int hosted_uart_read(uint8_t *data, uint16_t size)
{
	return 0;
}

int hosted_uart_write(uint8_t *data, uint16_t size)
{
	return 0;
}

void * hosted_uart_init(void)
{
	return NULL;
}

esp_err_t hosted_uart_deinit(void *ctx)
{
	return ESP_OK;
}
