// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/** Includes **/
#include <stdint.h>
#include <string.h>
#include "rtthread.h"
#include "rtdevice.h"
#include "string.h"
#include "common/trace.h"
#include "spi_drv.h"
#include "adapter.h"
#include "serial_drv.h"
#include "netdev_if.h"
#include "common/stats.h"

#define DBG_TAG           "esp.spi"
#define DBG_LVL           DBG_INFO
#include "rtdbg.h"


#define MAX_PAYLOAD_SIZE (MAX_SPI_BUFFER_SIZE - sizeof(struct esp_payload_header))


static struct esp_private *esp_priv[MAX_NETWORK_INTERFACES];

static struct netdev_ops esp_net_ops = {
	.netdev_open = esp_netdev_open,
	.netdev_close = esp_netdev_close,
	.netdev_xmit = esp_netdev_xmit,
};

/** Exported variables **/

static struct rt_spi_device *spi_dev;
static rt_sem_t osSemaphore;
static rt_mutex_t mutex_spi_trans;

static rt_thread_t process_rx_task_id = 0;
static rt_thread_t transaction_task_id = 0;

/* Queue declaration */
static rt_mq_t to_slave_queue = NULL;
static rt_mq_t from_slave_queue = NULL;

/* callback of event handler */
static void (*spi_drv_evt_handler_fp) (uint8_t);

/** function declaration **/
/** Exported functions **/
static stm_ret_t spi_transaction(uint8_t * txbuff);
static void transaction_task(void* pvParameters);
static void process_rx_task(void* pvParameters);
static uint8_t * get_tx_buffer(uint8_t *is_valid_tx_buf);
static void deinit_netdev(void);

/** Local Functions **/
/**
  * @brief  get private interface of expected type and number
  * @param  if_type - interface type
  *         if_num - interface number
  * @retval interface handle if found, else NULL
  */
static struct esp_private * get_priv(uint8_t if_type, uint8_t if_num)
{
	int i = 0;

	for (i = 0; i < MAX_NETWORK_INTERFACES; i++) {
		if((esp_priv[i]) &&
			(esp_priv[i]->if_type == if_type) &&
			(esp_priv[i]->if_num == if_num))
			return esp_priv[i];
	}

	return NULL;
}

/**
  * @brief  create virtual network device
  * @param  None
  * @retval None
  */
static int init_netdev(void)
{
	void *ndev = NULL;
	int i = 0;
	struct esp_private *priv = NULL;
	char *if_name = STA_INTERFACE;
	uint8_t if_type = ESP_STA_IF;

	for (i = 0; i < MAX_NETWORK_INTERFACES; i++) {
		/* Alloc and init netdev */
		ndev = netdev_stub_alloc(sizeof(struct esp_private), if_name);
		if (!ndev) {
			deinit_netdev();
			return STM_FAIL;
		}

		priv = (struct esp_private *) netdev_stub_get_priv(ndev);
		if (!priv) {
			deinit_netdev();
			return STM_FAIL;
		}

		priv->netdev = ndev;
		priv->if_type = if_type;
		priv->if_num = 0;

		if (netdev_stub_register(ndev, &esp_net_ops)) {
			deinit_netdev();
			return STM_FAIL;
		}

		if_name = SOFTAP_INTERFACE;
		if_type = ESP_AP_IF;

		esp_priv[i] = priv;
	}

	return STM_OK;
}

/**
  * @brief  destroy virtual network device
  * @param  None
  * @retval None
  */
static void deinit_netdev(void)
{
	for (int i = 0; i < MAX_NETWORK_INTERFACES; i++) {
		if (esp_priv[i]) {
			if (esp_priv[i]->netdev) {
				netdev_stub_unregister(esp_priv[i]->netdev);
				netdev_stub_free(esp_priv[i]->netdev);
			}
			esp_priv[i] = NULL;
		}
	}
}

/**
  * @brief EXTI line detection callback, used as SPI handshake GPIO
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
static void gpio_interrupt(void *args)
{
	/* Post semaphore to notify SPI slave is ready for next transaction */
	if (osSemaphore != NULL) {
		rt_sem_release(osSemaphore);
	}
}

/**
  * @brief  transport initializes
  * @param  transport_evt_handler_fp - event handler
  * @retval None
  */
void transport_init(void(*transport_evt_handler_fp)(uint8_t))
{
	stm_ret_t retval = STM_OK;

	/* register callback */
	spi_drv_evt_handler_fp = transport_evt_handler_fp;

	retval = (stm_ret_t)init_netdev();
	if (retval) {
		LOG_E("netdev failed to init");
		assert(retval==STM_OK);
	}

	/* spi handshake semaphore */
	osSemaphore = rt_sem_create("esp_hs", 0, RT_IPC_FLAG_PRIO);
	assert(osSemaphore);

	mutex_spi_trans = rt_mutex_create("esp_spi", RT_IPC_FLAG_PRIO);
	assert(mutex_spi_trans);

	/* Queue - tx */
	to_slave_queue = rt_mq_create("esp_spi_tx", sizeof(interface_buffer_handle_t), ESP_HOSTED_SPI_QUEUE_SIZE, RT_IPC_FLAG_PRIO);
	assert(to_slave_queue);

	/* Queue - rx */
	from_slave_queue = rt_mq_create("esp_spi_rx", sizeof(interface_buffer_handle_t), ESP_HOSTED_SPI_QUEUE_SIZE, RT_IPC_FLAG_PRIO);
	assert(from_slave_queue);

	/* Task - SPI transaction (full duplex) */
	transaction_task_id = rt_thread_create("esp_spi_tx", transaction_task, NULL, ESP_HOSTED_SPI_THREAD_STACK_SIZE, ESP_HOSTED_SPI_THREAD_PRIORITY, 2);
	assert(transaction_task_id);
	rt_thread_startup(transaction_task_id);

	/* Task - RX processing */
	process_rx_task_id = rt_thread_create("esp_spi_rx", process_rx_task, NULL, ESP_HOSTED_SPI_THREAD_STACK_SIZE, ESP_HOSTED_SPI_THREAD_PRIORITY, 2);
	assert(process_rx_task_id);
	rt_thread_startup(process_rx_task_id);

	/* Initializes the spi bus */
	if ((spi_dev = (struct rt_spi_device *)rt_device_find(ESP_HOSTED_SPI_DEVICE_NAME)) != NULL)
	{
		/* Configure SPI bus */
		struct rt_spi_configuration cfg;
        cfg.data_width = 8;
        cfg.mode = RT_SPI_MODE_2 | RT_SPI_MSB;
        cfg.max_hz = ESP_HOSTED_SPI_MAX_HZ;
        rt_spi_configure(spi_dev, &cfg);
	}
	else
	{
		LOG_E("No spi device (%s) is found. Please attach the spi bus!", ESP_HOSTED_SPI_DEVICE_NAME);
		return ;
	}

	/* Initializes an external interrupt */
	rt_pin_mode(ESP_HOSTED_HANDSHAKE_PIN, PIN_MODE_INPUT_PULLUP);
	rt_pin_mode(ESP_HOSTED_DATA_READY_PIN, PIN_MODE_INPUT_PULLUP);
	rt_pin_attach_irq(ESP_HOSTED_HANDSHAKE_PIN, PIN_IRQ_MODE_RISING, gpio_interrupt, NULL);
	rt_pin_attach_irq(ESP_HOSTED_DATA_READY_PIN, PIN_IRQ_MODE_RISING, gpio_interrupt, NULL);
	rt_pin_irq_enable(ESP_HOSTED_HANDSHAKE_PIN, RT_TRUE);
	rt_pin_irq_enable(ESP_HOSTED_DATA_READY_PIN, RT_TRUE);
}

/**
  * @brief  Schedule SPI transaction if -
  *         a. valid TX buffer is ready at SPI host (STM)
  *         b. valid TX buffer is ready at SPI peripheral (ESP)
  *         c. Dummy transaction is expected from SPI peripheral (ESP)
  * @param  argument: Not used
  * @retval None
  */
static void check_and_execute_spi_transaction(void)
{
	uint8_t * txbuff = NULL;
	uint8_t is_valid_tx_buf = 0;
	rt_bool_t gpio_handshake = RT_FALSE;
	rt_bool_t gpio_rx_data_ready = RT_FALSE;

	rt_mutex_take(mutex_spi_trans, RT_WAITING_FOREVER);

	/* handshake line SET -> slave ready for next transaction */
	gpio_handshake = rt_pin_read(ESP_HOSTED_HANDSHAKE_PIN);

	/* data ready line SET -> slave wants to send something */
	gpio_rx_data_ready = rt_pin_read(ESP_HOSTED_DATA_READY_PIN);

	if (gpio_handshake)
	{
		/* Get next tx buffer to be sent */
		txbuff = get_tx_buffer(&is_valid_tx_buf);

		if ( (gpio_rx_data_ready) || (is_valid_tx_buf) )
		{
			/* Execute transaction only if EITHER holds true-
			 * a. A valid tx buffer to be transmitted towards slave
			 * b. Slave wants to send something (Rx for host)
			 */
			spi_transaction(txbuff);
		}
	}

	rt_mutex_release(mutex_spi_trans);
}

/**
  * @brief  Send to slave via SPI
  * @param  iface_type -type of interface
  *         iface_num - interface number
  *         wbuffer - tx buffer
  *         wlen - size of wbuffer
  * @retval sendbuf - Tx buffer
  */
stm_ret_t send_to_slave(uint8_t iface_type, uint8_t iface_num,
		uint8_t * wbuffer, uint16_t wlen)
{
	interface_buffer_handle_t buf_handle = {0};

	if (!wbuffer || !wlen || (wlen > MAX_PAYLOAD_SIZE)) {
		LOG_E("write fail: buff(%p) 0? OR (0<len(%u)<=max_poss_len(%u))?",
				wbuffer, wlen, MAX_PAYLOAD_SIZE);
		if(wbuffer) {
			free(wbuffer);
			wbuffer = NULL;
		}
		return STM_FAIL;
	}
	memset(&buf_handle, 0, sizeof(buf_handle));

	buf_handle.if_type = iface_type;
	buf_handle.if_num = iface_num;
	buf_handle.payload_len = wlen;
	buf_handle.payload = wbuffer;
	buf_handle.priv_buffer_handle = wbuffer;
	buf_handle.free_buf_handle = free;

	if (RT_EOK != rt_mq_send_wait(to_slave_queue, &buf_handle, sizeof(buf_handle), RT_WAITING_FOREVER)) {
		LOG_E("Failed to send buffer to_slave_queue");
		if(wbuffer) {
			free(wbuffer);
			wbuffer = NULL;
		}
		return STM_FAIL;
	}

	check_and_execute_spi_transaction();

	return STM_OK;
}

/** Local functions **/

/**
  * @brief  Give breathing time for slave on spi
  * @param  x - for loop delay count
  * @retval None
  */
static void stop_spi_transactions_for_msec(int x)
{
	hard_delay(x);
}

/**
  * @brief  Full duplex transaction SPI transaction for ESP32S2 hardware
  * @param  txbuff: TX SPI buffer
  * @retval STM_OK / STM_FAIL
  */
static stm_ret_t spi_transaction(uint8_t * txbuff)
{
	uint8_t *rxbuff = NULL;
	interface_buffer_handle_t buf_handle = {0};
	struct  esp_payload_header *payload_header;
	uint16_t len, offset;
	rt_size_t rx_length;
	uint16_t rx_checksum = 0, checksum = 0;

	/* Allocate rx buffer */
	rxbuff = (uint8_t *)malloc(MAX_SPI_BUFFER_SIZE);
	assert(rxbuff);
	memset(rxbuff, 0, MAX_SPI_BUFFER_SIZE);

	if(!txbuff) {
		/* Even though, there is nothing to send,
		 * valid reseted txbuff is needed for SPI driver
		 */
		txbuff = (uint8_t *)malloc(MAX_SPI_BUFFER_SIZE);
		assert(txbuff);
		memset(txbuff, 0, MAX_SPI_BUFFER_SIZE);
	}

	/* SPI transaction */
	rx_length = rt_spi_transfer(spi_dev, txbuff, rxbuff, MAX_SPI_BUFFER_SIZE);

	if (rx_length != 0)
	{
		/* Transaction successful */

		/* create buffer rx handle, used for processing */
		payload_header = (struct esp_payload_header *) rxbuff;

		/* Fetch length and offset from payload header */
		len = le16toh(payload_header->len);
		offset = le16toh(payload_header->offset);

		if ((!len) || (len > MAX_PAYLOAD_SIZE) || (offset != sizeof(struct esp_payload_header))) 
		{
			/* Free up buffer, as one of following -
				* 1. no payload to process
				* 2. input packet size > driver capacity
				* 3. payload header size mismatch,
				* wrong header/bit packing?
				* */
			if (rxbuff) {
				free(rxbuff);
				rxbuff = NULL;
			}
			
			/* Give chance to other tasks */
			rt_thread_yield();
		}
		else
		{
			rx_checksum = le16toh(payload_header->checksum);
			payload_header->checksum = 0;

			checksum = compute_checksum(rxbuff, len+offset);

			if (checksum == rx_checksum) {
				buf_handle.priv_buffer_handle = rxbuff;
				buf_handle.free_buf_handle = free;
				buf_handle.payload_len = len;
				buf_handle.if_type     = payload_header->if_type;
				buf_handle.if_num      = payload_header->if_num;
				buf_handle.payload     = rxbuff + offset;
				buf_handle.seq_num     = le16toh(payload_header->seq_num);
				buf_handle.flag        = payload_header->flags;

				if (RT_EOK != rt_mq_send_wait(from_slave_queue, &buf_handle, sizeof(buf_handle), RT_WAITING_FOREVER)) {
					LOG_E("Failed to send buffer");
					goto done;
				}
			} else {
				if (rxbuff) {
					free(rxbuff);
					rxbuff = NULL;
				}
			}
		}

		/* Free input TX buffer */
		if (txbuff) {
			free(txbuff);
			txbuff = NULL;
		}
	}
	else
	{
		goto done;
	}

	return STM_OK;

done:
	/* error cases, abort */
	if (txbuff) {
		free(txbuff);
		txbuff = NULL;
	}

	if (rxbuff) {
		free(rxbuff);
		rxbuff = NULL;
	}
	return STM_FAIL;
}

/**
  * @brief  Task for SPI transaction
  * @param  argument: Not used
  * @retval None
  */
static void transaction_task(void* pvParameters)
{
	for (;;) {

		if (osSemaphore != NULL) {
			/* Wait till slave is ready for next transaction */
			if (rt_sem_take(osSemaphore, RT_WAITING_FOREVER) == RT_EOK) {
				check_and_execute_spi_transaction();
			}
		}
	}
}

/**
  * @brief  RX processing task
  * @param  argument: Not used
  * @retval None
  */
static void process_rx_task(void* pvParameters)
{
	interface_buffer_handle_t buf_handle = {0};
	uint8_t *payload = NULL;
	struct pbuf *buffer = NULL;
	struct esp_priv_event *event = NULL;
	struct esp_private *priv = NULL;

	while (1) {

		if (rt_mq_recv(from_slave_queue, &buf_handle, sizeof(buf_handle), RT_WAITING_FOREVER) != RT_EOK) {
			continue;
		}

		/* point to payload */
		payload = buf_handle.payload;

		/* process received buffer for all possible interface types */
		if (buf_handle.if_type == ESP_SERIAL_IF) {

			/* serial interface path */
			serial_rx_handler(&buf_handle);

		} else if((buf_handle.if_type == ESP_STA_IF) ||
				(buf_handle.if_type == ESP_AP_IF)) {
			priv = get_priv(buf_handle.if_type, buf_handle.if_num);

			if (priv) {
				buffer = (struct pbuf *)malloc(sizeof(struct pbuf));
				assert(buffer);

				buffer->len = buf_handle.payload_len;
				buffer->payload = malloc(buf_handle.payload_len);
				assert(buffer->payload);

				memcpy(buffer->payload, buf_handle.payload,
						buf_handle.payload_len);

				netdev_stub_rx(priv->netdev, buffer);
			}

		} else if (buf_handle.if_type == ESP_PRIV_IF) {
			buffer = (struct pbuf *)malloc(sizeof(struct pbuf));
			assert(buffer);

			buffer->len = buf_handle.payload_len;
			buffer->payload = malloc(buf_handle.payload_len);
			assert(buffer->payload);

			memcpy(buffer->payload, buf_handle.payload,
					buf_handle.payload_len);

			process_priv_communication(buffer);
			/* priv transaction received */
			LOG_D("Received INIT event");

			event = (struct esp_priv_event *) (payload);
			if (event->event_type == ESP_PRIV_EVENT_INIT) {
				/* halt spi transactions for some time,
				 * this is one time delay, to give breathing
				 * time to slave before spi trans start */
				stop_spi_transactions_for_msec(50000);
				if (spi_drv_evt_handler_fp) {
					spi_drv_evt_handler_fp(TRANSPORT_ACTIVE);
				}
			} else {
				/* User can re-use this type of transaction */
			}
		} else if (buf_handle.if_type == ESP_TEST_IF) {
#if TEST_RAW_TP
			update_test_raw_tp_rx_len(buf_handle.payload_len);
#endif
		} else {
			LOG_E("unknown type %d ", buf_handle.if_type);
		}

		/* Free buffer handle */
		/* When buffer offloaded to other module, that module is
		 * responsible for freeing buffer. In case not offloaded or
		 * failed to offload, buffer should be freed here.
		 */
		if (buf_handle.free_buf_handle) {
			buf_handle.free_buf_handle(buf_handle.priv_buffer_handle);
		}
	}
}


/**
  * @brief  Next TX buffer in SPI transaction
  * @param  argument: Not used
  * @retval sendbuf - Tx buffer
  */
static uint8_t * get_tx_buffer(uint8_t *is_valid_tx_buf)
{
	struct  esp_payload_header *payload_header;
	uint8_t *sendbuf = NULL;
	uint8_t *payload = NULL;
	uint16_t len = 0;
	interface_buffer_handle_t buf_handle = {0};

	*is_valid_tx_buf = 0;

	/* Check if higher layers have anything to transmit, non blocking.
	 * If nothing is expected to send, queue receive will fail.
	 * In that case only payload header with zero payload
	 * length would be transmitted.
	 */
	if (RT_EOK == rt_mq_recv(to_slave_queue, &buf_handle, sizeof(buf_handle), 0)) {
		len = buf_handle.payload_len;
	}

	if (len) {

		sendbuf = (uint8_t *) malloc(MAX_SPI_BUFFER_SIZE);
		if (!sendbuf) {
			LOG_E("malloc failed");
			goto done;
		}

		memset(sendbuf, 0, MAX_SPI_BUFFER_SIZE);

		*is_valid_tx_buf = 1;

		/* Form Tx header */
		payload_header = (struct esp_payload_header *) sendbuf;
		payload = sendbuf + sizeof(struct esp_payload_header);
		payload_header->len     = htole16(len);
		payload_header->offset  = htole16(sizeof(struct esp_payload_header));
		payload_header->if_type = buf_handle.if_type;
		payload_header->if_num  = buf_handle.if_num;
		memcpy(payload, buf_handle.payload, min(len, MAX_PAYLOAD_SIZE));
		payload_header->checksum = htole16(compute_checksum(sendbuf,
				sizeof(struct esp_payload_header)+len));;
	}

done:
	/* free allocated buffer */
	if (buf_handle.free_buf_handle)
		buf_handle.free_buf_handle(buf_handle.priv_buffer_handle);

	return sendbuf;
}
