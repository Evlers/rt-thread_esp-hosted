#ifndef __ESP_HOSTED_CONFIG_H__
#define __ESP_HOSTED_CONFIG_H__

#include <stdbool.h>

#include "main.h"
#include "esp_task.h"
#include "esp_err.h"

/* This file is to tune the main ESP-Hosted configurations.
 * In case you are not sure of some value, Let it be default.
 **/

#define H_TRANSPORT_NONE 0
#define H_TRANSPORT_SDIO 1
#define H_TRANSPORT_SPI_HD 2
#define H_TRANSPORT_SPI 3
#define H_TRANSPORT_UART 4

#define H_TRANSPORT_IN_USE H_TRANSPORT_SPI

/*  ========================== SPI Master Config start ======================  */
/*
Pins in use. The SPI Master can use the GPIO mux,
so feel free to change these if needed.
*/


enum {
	SLAVE_CHIPSET_ESP32,
	SLAVE_CHIPSET_ESP32C2,
	SLAVE_CHIPSET_ESP32C3,
	SLAVE_CHIPSET_ESP32C6,
	SLAVE_CHIPSET_ESP32S2,
	SLAVE_CHIPSET_ESP32S3,
	SLAVE_CHIPSET_MAX_SUPPORTED
};

/* Select the slave chipset: only enable one */
#define CONFIG_SLAVE_CHIPSET_ESP32 1
// #define CONFIG_SLAVE_CHIPSET_ESP32C2 1
// #define CONFIG_SLAVE_CHIPSET_ESP32C3 1
// #define CONFIG_SLAVE_CHIPSET_ESP32C5 1
// #define CONFIG_SLAVE_CHIPSET_ESP32C6 1
// #define CONFIG_SLAVE_CHIPSET_ESP32S2 1
// #define CONFIG_SLAVE_CHIPSET_ESP32S3 1

/* Use manual or Hardware NSS for SPI CS
 * disable both when using SPi INTR */
// #define STM32_MANUAL_CS 1
// #define STM32_MANUAL_CS 0
/* For now, interrupt driven SPI transaction is known to work for STM32H7XX */
#define STM32_SPI_INTR 1

/* Log level to be set:
 * 5 - verbose
 * 4 - debug
 * 3 - info
 * 2 - warn
 * 1 - err
 *
 * Higher the level, higher the logs
 */
#define CONFIG_LOG_MAXIMUM_LEVEL 3

/* Mempool for spi transport allocations */
// #define H_USE_MEMPOOL 1
#define H_USE_MEMPOOL 0
/* Mmmory alignement for mempool blocks */
#define H_DMA_OR_CACHE_ALIGNMENT_BYTES   4

#define CONFIG_ESP_SPI_HOST_INTERFACE 1

#if CONFIG_ESP_SPI_HOST_INTERFACE
/* This is needed as For STM32, we have two variants for SPI transaction
 * 1. Using Cs pin from SPI peripheral
 * 2. Manual CS switching for GPIO
 */

/* SPI instance used */
#if defined(STM32F469xx)
	#define SPI_BUS_HAL hspi1
#elif defined(STM32H743xx)
	#define SPI_BUS_HAL hspi3
#else
	#error "Which SPI instance you want to use?"
	#error "Please cross-check ioc file for your STM32 & Data sheet"
	#error "If non STM MCU, please port os_wrapper.c, os_wrapper.h, os_header.h files"
#endif
#endif

#ifdef STM32F469xx
/* Comment for STM32-F4
 * This is just to print values.
 * For MCU as STM32, you cannot change values for these pins here
 * as they are needed to be configured by changing ioc file.
 * Please open ioc file using STM32CubeIDE and change if need be
 */

#ifndef GPIO_HANDSHAKE_Pin
	#define GPIO_HANDSHAKE_Pin GPIO_PIN_6
	#define GPIO_HANDSHAKE_GPIO_Port GPIOC
#endif

#ifndef GPIO_RESET_Pin
	#define GPIO_RESET_Pin GPIO_PIN_13
	#define GPIO_RESET_GPIO_Port GPIOB
#endif

#if STM32_MANUAL_CS
#ifndef USR_SPI_CS_Pin
	#define USR_SPI_CS_Pin GPIO_PIN_15
	#define USR_SPI_CS_GPIO_Port GPIOA
#endif
#endif

#ifndef GPIO_DATA_READY_Pin
	#define GPIO_DATA_READY_Pin GPIO_PIN_7
	#define GPIO_DATA_READY_GPIO_Port GPIOC
#endif

#ifndef GPIO_DATA_READY_EXTI_IRQn
	#define GPIO_DATA_READY_EXTI_IRQn EXTI9_5_IRQn
#endif

#ifndef GPIO_HANDSHAKE_EXTI_IRQn
	#define GPIO_HANDSHAKE_EXTI_IRQn EXTI9_5_IRQn
#endif


#ifndef GPIO_MOSI_Pin
	#define GPIO_MOSI_Pin GPIO_PIN_5
	#define GPIO_MOSI_GPIO_Port GPIOB
#endif

#ifndef GPIO_MISO_Pin
	#define GPIO_MISO_Pin GPIO_PIN_4
	#define GPIO_MISO_GPIO_Port GPIOB
#endif

#ifndef GPIO_CLK_Pin
	#define GPIO_CLK_Pin GPIO_PIN_5
	#define GPIO_CLK_GPIO_Port GPIOA
#endif



#elif defined(STM32H743xx)
/* Comment for STM32-H7
 * This is just to print values.
 * For MCU as STM32, you cannot change values for these pins here
 * as they are needed to be configured by changing ioc file.
 * Please open ioc file using STM32CubeIDE and change if need be
 */


#ifndef GPIO_HANDSHAKE_Pin
	/* CN8 16 */
	#define GPIO_HANDSHAKE_Pin GPIO_PIN_3
	#define GPIO_HANDSHAKE_GPIO_Port GPIOG
#endif

#ifndef GPIO_RESET_Pin
	/* CN7 20 */
	#define GPIO_RESET_Pin GPIO_PIN_3
	#define GPIO_RESET_GPIO_Port GPIOF
#endif

#ifndef USR_SPI_CS_Pin
	/* CN7 16 */
	#define USR_SPI_CS_Pin GPIO_PIN_9
	#define USR_SPI_CS_GPIO_Port GPIOB
#endif

#ifndef GPIO_DATA_READY_Pin
	/* CN8 14 */
	#define GPIO_DATA_READY_Pin GPIO_PIN_2
	#define GPIO_DATA_READY_GPIO_Port GPIOG
#endif

#ifndef GPIO_DATA_READY_EXTI_IRQn
	#define GPIO_DATA_READY_EXTI_IRQn EXTI2_IRQn
#endif

#ifndef GPIO_HANDSHAKE_EXTI_IRQn
	#define GPIO_HANDSHAKE_EXTI_IRQn EXTI3_IRQn
#endif

#ifndef GPIO_MOSI_Pin
	/* CN7 13 */
	#define GPIO_MOSI_Pin GPIO_PIN_5
	#define GPIO_MOSI_GPIO_Port GPIOB
#endif


#ifndef GPIO_MISO_Pin
	/* CN7 19 */
	#define GPIO_MISO_Pin GPIO_PIN_4
	#define GPIO_MISO_GPIO_Port GPIOB
#endif

#ifndef GPIO_CLK_Pin
	/* CN7 15 */
	#define GPIO_CLK_Pin GPIO_PIN_3
	#define GPIO_CLK_GPIO_Port GPIOB
#endif

#ifndef GPIO_CS_Pin
	#define GPIO_CS_Pin USR_SPI_CS_Pin
	#define GPIO_CS_GPIO_Port USR_SPI_CS_GPIO_Port
#endif

/*
 *   +---------+-----------------------+-----------------------+----------+----------+
 *   |  Func   |    STM32F469I-DISCO   | STM32- NUCLEO H743ZI2 | ESP32-C3 | ESP32-C6 |
 *   +---------+-----------------------+-----------------------+----------+----------+
 *   |  MOSI   |       CN12  9  B5     |       CN7 13  B5      |   10     |   7      |
 *   |  MISO   |       CN12  5  B4     |       CN7 19  B4      |   18     |  20  (2)    |
 *   |  CLK    |       CN12  7  A5     |       CN7 15  B3      |    3     |   3  (6)    |
 *   |  CS     |       CN12 11  A15    |       CN7 16  D14     |   19     |  21  (10)    |
 *   |  HS     |       CN12  6  C6     |       CN8 16  G3      |    0     |   0  (3)    |
 *   |  DR     |       CN12  8  C7     |       CN8 14  G2      |    9     |   9  (4)    |
 *   | GND     |       CN12  2  GND    |       CN7  8  GND     |   GND    |  GND     |
 *   | Sl RES  |       CN12 10  B13    |       CN7 20  F3      |   RST    |  RST     |
 *   +---------+-----------------------+-----------------------+----------+----------+
 */

#else

#error "Please port above GPIO values for your MCU"

#endif

/* SPI - Handshake */
#define H_HS_VAL_ACTIVE                              H_GPIO_HIGH
#define H_HS_VAL_INACTIVE                            H_GPIO_LOW
#define H_HS_INTR_EDGE                               H_GPIO_INTR_POSEDGE

/* SPI - DataReady */
#define H_DR_VAL_ACTIVE                              H_GPIO_HIGH
#define H_DR_VAL_INACTIVE                            H_GPIO_LOW
#define H_DR_INTR_EDGE                               H_GPIO_INTR_POSEDGE


#define H_GPIO_HANDSHAKE_Port                        GPIO_HANDSHAKE_GPIO_Port
#define H_GPIO_HANDSHAKE_Pin                         GPIO_HANDSHAKE_Pin

#define H_GPIO_DATA_READY_Port                       GPIO_DATA_READY_GPIO_Port
#define H_GPIO_DATA_READY_Pin                        GPIO_DATA_READY_Pin

#define H_RESET_VAL_ACTIVE                           H_GPIO_HIGH
#define H_RESET_VAL_INACTIVE                         H_GPIO_LOW
#define H_GPIO_PIN_RESET_Port                        GPIO_RESET_GPIO_Port
#define H_GPIO_PIN_RESET_Pin                         GPIO_RESET_Pin

#define H_GPIO_LOW                                   0
#define H_GPIO_HIGH                                  1

#define H_SPI_TX_Q                                     20
#define H_SPI_RX_Q                                     20
#define H_SPI_MODE                                     1
#define H_SPI_INIT_CLK_MHZ                             10

/*  --------------------------- RPC Host Config start -----------------------  */
#define H_MAX_SIMULTANEOUS_SYNC_RPC_REQUESTS         5
#define H_MAX_SIMULTANEOUS_ASYNC_RPC_REQUESTS        5
#define H_DEFAULT_RPC_RSP_TIMEOUT                    5
/*  --------------------------- RPC Host Config end -------------------------  */

/* At slave, if Wi-Fi tx is failing (maybe due to unstable wifi connection),
 * as preventive measure, host can start dropping the packets, depending upon slave load
 * Typically, low threshold is 60, high threshold = 90 for spi, 80 for sdio
 * 0 means disable the feature
 */
#define H_WIFI_TX_DATA_THROTTLE_LOW_THRESHOLD        0
#define H_WIFI_TX_DATA_THROTTLE_HIGH_THRESHOLD       0

/* Raw Throughput Testing */
#define H_TEST_RAW_TP                                0
#define H_ESP_RAW_TP_REPORT_INTERVAL                 10
#define H_ESP_RAW_TP_HOST_TO_ESP_PKT_LEN             1460

/* Raw throughput direction
 * Raw throughput is pure transport performance, without involving Wi-Fi etc layers
 * - For transport test from host to slave, enable CONFIG_ESP_RAW_THROUGHPUT_TX_TO_SLAVE
 * - For transport test from slave to host, enable CONFIG_ESP_RAW_THROUGHPUT_RX_FROM_SLAVE
 * - For bi-directional transport test, enable CONFIG_ESP_RAW_THROUGHPUT_BIDIRECTIONAL
 */
// #define CONFIG_ESP_RAW_THROUGHPUT_TX_TO_SLAVE        1
// #define CONFIG_ESP_RAW_THROUGHPUT_RX_FROM_SLAVE      1
#define CONFIG_ESP_RAW_THROUGHPUT_BIDIRECTIONAL      1

/* Use or don't use NVS on co-processor to save WiFi Config */
#if CONFIG_ESP_WIFI_NVS_ENABLED
#define H_ESP_WIFI_NVS_ENABLED 1
#else
#define H_ESP_WIFI_NVS_ENABLED 0
#endif

#if H_TEST_RAW_TP
#if CONFIG_ESP_RAW_THROUGHPUT_TX_TO_SLAVE
#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP__HOST_TO_ESP)
#elif CONFIG_ESP_RAW_THROUGHPUT_RX_FROM_SLAVE
#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP__ESP_TO_HOST)
#elif CONFIG_ESP_RAW_THROUGHPUT_BIDIRECTIONAL
#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP__BIDIRECTIONAL)
#else
#error Test Raw TP direction not defined
#endif
#else
#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP_NONE)
#endif

/*  ========================== SPI Master Config end ========================  */

esp_err_t esp_hosted_set_default_config(void);
bool esp_hosted_is_config_valid(void);

#define PRE_FORMAT_NEWLINE_CHAR                      "\r"
#define POST_FORMAT_NEWLINE_CHAR                     "\n"

#define CONFIG_H_LOWER_MEMCOPY                       0

/* Do not use standard c malloc and hook to freertos heap4 */
#define USE_STD_C_LIB_MALLOC                         1

#define H_MEM_STATS                                  0 // disable for now

#define H_ESP_PKT_STATS                              1

/* netif & lwip or rest config included from sdkconfig */
#include "sdkconfig.h"

#endif /*__ESP_HOSTED_CONFIG_H__*/
