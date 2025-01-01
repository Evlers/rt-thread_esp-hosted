/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#ifndef __ESP_HOSTED_CONFIG_H__
#define __ESP_HOSTED_CONFIG_H__

#include "config_convert.h"
#include "os_wrapper.h"
#include "hosted_os_adapter.h"
#include "adapter.h"
#include "esp_err.h"

#if CONFIG_SLAVE_CHIPSET_ESP32C2
#define ESP_WIFI_MAX_CONN_NUM  (4)        /**< max number of stations which can connect to ESP32C2 soft-AP */
#elif CONFIG_SLAVE_CHIPSET_ESP32C3 || CONFIG_SLAVE_CHIPSET_ESP32C6 || CONFIG_SLAVE_CHIPSET_ESP32C5 || CONFIG_SLAVE_CHIPSET_ESP32C61
#define ESP_WIFI_MAX_CONN_NUM  (10)       /**< max number of stations which can connect to ESP32C3/ESP32C6/ESP32C5/ESP32C61 soft-AP */
#else
#define ESP_WIFI_MAX_CONN_NUM  (15)       /**< max number of stations which can connect to ESP32/ESP32S3/ESP32S2 soft-AP */
#endif

#define CONFIG_LOG_DEFAULT_LEVEL  ESP_LOG_INFO
#define CONFIG_LOG_MAXIMUM_LEVEL  ESP_LOG_INFO

#define H_TRANSPORT_NONE      0
#define H_TRANSPORT_SDIO      1
#define H_TRANSPORT_SPI_HD    2
#define H_TRANSPORT_SPI       3
#define H_TRANSPORT_UART      4

#undef H_TRANSPORT_IN_USE

#if CONFIG_ESP_SPI_HOST_INTERFACE
#define H_TRANSPORT_IN_USE H_TRANSPORT_SPI
/*  -------------------------- SPI Master Config start ----------------------  */

#ifdef CONFIG_HS_ACTIVE_LOW
  #define H_HANDSHAKE_ACTIVE_HIGH 0
#else
  /* Default HS: Active High */
  #define H_HANDSHAKE_ACTIVE_HIGH 1
#endif

#ifdef CONFIG_DR_ACTIVE_LOW
  #define H_DATAREADY_ACTIVE_HIGH 0
#else
  /* Default DR: Active High */
  #define H_DATAREADY_ACTIVE_HIGH 1
#endif

#if H_HANDSHAKE_ACTIVE_HIGH
  #define H_HS_VAL_ACTIVE                            H_GPIO_HIGH
  #define H_HS_VAL_INACTIVE                          H_GPIO_LOW
  #define H_HS_INTR_EDGE                             H_GPIO_INTR_POSEDGE
#else
  #define H_HS_VAL_ACTIVE                            H_GPIO_LOW
  #define H_HS_VAL_INACTIVE                          H_GPIO_HIGH
  #define H_HS_INTR_EDGE                             H_GPIO_INTR_NEGEDGE
#endif

#if H_DATAREADY_ACTIVE_HIGH
  #define H_DR_VAL_ACTIVE                            H_GPIO_HIGH
  #define H_DR_VAL_INACTIVE                          H_GPIO_LOW
  #define H_DR_INTR_EDGE                             H_GPIO_INTR_POSEDGE
#else
  #define H_DR_VAL_ACTIVE                            H_GPIO_LOW
  #define H_DR_VAL_INACTIVE                          H_GPIO_HIGH
  #define H_DR_INTR_EDGE                             H_GPIO_INTR_NEGEDGE
#endif

#define H_GPIO_HANDSHAKE_Port                        NULL
#define H_GPIO_HANDSHAKE_Pin                         ESP_HOSTED_HANDSHAKE_PIN
#define H_GPIO_DATA_READY_Port                       NULL
#define H_GPIO_DATA_READY_Pin                        ESP_HOSTED_DATA_READY_PIN



#define H_GPIO_MOSI_Port                             NULL
#define H_GPIO_MOSI_Pin                              -1
#define H_GPIO_MISO_Port                             NULL
#define H_GPIO_MISO_Pin                              -1
#define H_GPIO_SCLK_Port                             NULL
#define H_GPIO_SCLK_Pin                              -1
#define H_GPIO_CS_Port                               NULL
#define H_GPIO_CS_Pin                                ESP_HOSTED_SPI_CS_PIN

#define H_SPI_TX_Q                                   1
#define H_SPI_RX_Q                                   1

#define H_SPI_MODE                                   RT_SPI_MODE_3
#define H_SPI_INIT_CLK_MHZ                           ESP_HOSTED_SPI_MAX_HZ

/*  -------------------------- SPI Master Config end ------------------------  */
#endif

/* Generic reset pin config */
#define H_GPIO_PIN_RESET_Port                         NULL
#define H_GPIO_PIN_RESET_Pin                          ESP_HOSTED_RESET_PIN

/* If Reset pin is Enable, it is Active High.
 * If it is RST, active low */
#ifdef CONFIG_RESET_GPIO_ACTIVE_LOW
  #define H_RESET_ACTIVE_HIGH                         0
#else
  #define H_RESET_ACTIVE_HIGH                         1
#endif

#ifdef H_RESET_ACTIVE_HIGH
  #define H_RESET_VAL_ACTIVE                          H_GPIO_HIGH
  #define H_RESET_VAL_INACTIVE                        H_GPIO_LOW
#else
  #define H_RESET_VAL_ACTIVE                          H_GPIO_LOW
  #define H_RESET_VAL_INACTIVE                        H_GPIO_HIGH
#endif

#define PRE_FORMAT_NEWLINE_CHAR                       ""
#define POST_FORMAT_NEWLINE_CHAR                      ""

/* At slave, if Wi-Fi tx is failing (maybe due to unstable wifi connection),
 * as preventive measure, host can start dropping the packets, depending upon slave load
 * Typically, low threshold is 60, high threshold = 90 for spi, 80 for sdio
 * 0 means disable the feature
 */
#ifdef CONFIG_HOST_TO_ESP_WIFI_DATA_THROTTLE
  #define H_WIFI_TX_DATA_THROTTLE_LOW_THRESHOLD        CONFIG_TO_WIFI_DATA_THROTTLE_LOW_THRESHOLD
  #define H_WIFI_TX_DATA_THROTTLE_HIGH_THRESHOLD       CONFIG_TO_WIFI_DATA_THROTTLE_HIGH_THRESHOLD
#else
  #define H_WIFI_TX_DATA_THROTTLE_LOW_THRESHOLD        0
  #define H_WIFI_TX_DATA_THROTTLE_HIGH_THRESHOLD       0
#endif

/* Raw Throughput Testing */
#ifdef ESP_HOSTED_RAW_THROUGHPUT_TRANSPORT

#define H_TEST_RAW_TP                                  1
#define H_ESP_RAW_TP_REPORT_INTERVAL                   ESP_HOSTED_RAW_TP_REPORT_INTERVAL
#define H_ESP_RAW_TP_HOST_TO_ESP_PKT_LEN               ESP_HOSTED_RAW_TP_HOST_TO_ESP_PKT_LEN

#if defined(ESP_HOSTED_RAW_THROUGHPUT_TX_TO_SLAVE)
#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP__HOST_TO_ESP)
#elif defined(ESP_HOSTED_RAW_THROUGHPUT_RX_FROM_SLAVE)
#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP__ESP_TO_HOST)
#elif defined(ESP_HOSTED_RAW_THROUGHPUT_BIDIRECTIONAL)
#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP__BIDIRECTIONAL)
#else
#error Test Raw TP direction not defined
#endif

#else

#define H_TEST_RAW_TP                                   0
#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP_NONE)

#endif /* ESP_HOSTED_RAW_THROUGHPUT_TRANSPORT */

#define H_TRANSPORT_QUEUE_NUMBER                       ESP_HOSTED_TRANSPORT_QUEUE_NUMBER
#define H_MAX_SIMULTANEOUS_SYNC_RPC_REQUESTS           ESP_HOSTED_MAX_SIMULTANEOUS_SYNC_RPC_REQUESTS
#define H_MAX_SIMULTANEOUS_ASYNC_RPC_REQUESTS          ESP_HOSTED_MAX_SIMULTANEOUS_ASYNC_RPC_REQUESTS

#define H_ERROR_CHECK(x) do {                                                                                           \
                                int err_rc_ = (x);                                                                      \
                                if (unlikely(err_rc_ != 0)) {                                                           \
                                    ESP_LOGE("esp-hosted", "Error %d (%s) at %s:%d", err_rc_, #x, __FILE__, __LINE__);  \
                                    abort();                                                                            \
                                }                                                                                       \
                            } while(0)

esp_err_t esp_hosted_set_default_config(void);
bool esp_hosted_is_config_valid(void);

#endif /*__ESP_HOSTED_CONFIG_H__*/
