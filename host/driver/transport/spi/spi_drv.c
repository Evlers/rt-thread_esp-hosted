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
#include "common/stats.h"
#include "common/trace.h"
#include "common/common.h"
#include "transport_drv.h"
#include "adapter.h"
#include "serial_drv.h"
#include "netdev_if.h"


#define DBG_TAG           "esp.spi"
#define DBG_LVL           DBG_ERROR
#include "rtdbg.h"


#define MAX_PAYLOAD_SIZE (MAX_SPI_BUFFER_SIZE - sizeof(struct esp_payload_header))


static struct esp_private *esp_priv[MAX_NETWORK_INTERFACES];

static struct netdev_ops esp_net_ops =
{
    .netdev_open = esp_netdev_open,
    .netdev_close = esp_netdev_close,
    .netdev_xmit = esp_netdev_xmit,
};

/** Exported variables **/

static rt_sem_t trans_semaphore;

static rt_thread_t process_rx_thread_id = 0;
static rt_thread_t transaction_thread_id = 0;

/* Queue declaration */
static rt_mq_t to_slave_queue = NULL;
static rt_mq_t from_slave_queue = NULL;

/* callback of event handler */
static void (*spi_drv_evt_handler_fp) (uint8_t);

/** function declaration **/
/** Exported functions **/
static void transaction_thread(void *parameter);
static void process_rx_thread(void *parameter);
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

    for (i = 0; i < MAX_NETWORK_INTERFACES; i++)
    {
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

    for (i = 0; i < MAX_NETWORK_INTERFACES; i++)
    {
        /* Alloc and init netdev */
        ndev = netdev_stub_alloc(sizeof(struct esp_private), if_name);
        if (!ndev) {
            deinit_netdev();
            return ESP_FAIL;
        }

        priv = (struct esp_private *) netdev_stub_get_priv(ndev);
        if (!priv) {
            deinit_netdev();
            return ESP_FAIL;
        }

        priv->netdev = ndev;
        priv->if_type = if_type;
        priv->if_num = 0;

        if (netdev_stub_register(ndev, &esp_net_ops)) {
            deinit_netdev();
            return ESP_FAIL;
        }

        if_name = SOFTAP_INTERFACE;
        if_type = ESP_AP_IF;

        esp_priv[i] = priv;
    }

    return ESP_OK;
}

/**
  * @brief  destroy virtual network device
  * @param  None
  * @retval None
  */
static void deinit_netdev(void)
{
    for (int i = 0; i < MAX_NETWORK_INTERFACES; i++)
    {
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
  * @brief EXTI line detection callback, used as SPI handshake&data_ready GPIO
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
static void gpio_interrupt(void *args)
{
    /* Post semaphore to notify SPI slave is ready for next transaction */
    if (trans_semaphore != NULL)
    {
        rt_sem_release(trans_semaphore);
    }
}

/**
  * @brief  transport initializes
  * @param  transport_evt_handler_fp - event handler
  * @retval None
  */
void transport_init(void(*transport_evt_handler_fp)(uint8_t))
{
    /* register callback */
    spi_drv_evt_handler_fp = transport_evt_handler_fp;

    if (init_netdev())
    {
        LOG_E("netdev failed to init");
        return ;
    }

    /* spi transter semaphore */
    trans_semaphore = rt_sem_create("esp_trans", 0, RT_IPC_FLAG_PRIO);
    assert(trans_semaphore);

    /* Queue - tx */
    to_slave_queue = rt_mq_create("esp_spi_tx", sizeof(interface_buffer_handle_t), ESP_HOSTED_SPI_QUEUE_SIZE, RT_IPC_FLAG_PRIO);
    assert(to_slave_queue);

    /* Queue - rx */
    from_slave_queue = rt_mq_create("esp_spi_rx", sizeof(interface_buffer_handle_t), ESP_HOSTED_SPI_QUEUE_SIZE, RT_IPC_FLAG_PRIO);
    assert(from_slave_queue);

    /* Initializes the spi bus */
    static struct rt_spi_device esp_spi_device;
    if (rt_spi_bus_attach_device_cspin(&esp_spi_device, ESP_HOSTED_SPI_DEVICE_NAME, ESP_HOSTED_SPI_BUS_NAME, ESP_HOSTED_SPI_CS_PIN, NULL) == RT_EOK)
    {
        /* Configure SPI bus */
        struct rt_spi_configuration cfg;
        cfg.data_width = 8;
        cfg.mode = RT_SPI_MODE_2 | RT_SPI_MSB;
        cfg.max_hz = ESP_HOSTED_SPI_MAX_HZ;
        rt_spi_configure(&esp_spi_device, &cfg);
    }
    else
    {
        LOG_E("No spi device (%s) is found. Please attach the spi bus!", ESP_HOSTED_SPI_DEVICE_NAME);
        return ;
    }

    /* Thread - SPI transaction (full duplex) */
    transaction_thread_id = rt_thread_create("esp_spi_tx", transaction_thread, &esp_spi_device, ESP_HOSTED_SPI_THREAD_STACK_SIZE, ESP_HOSTED_SPI_THREAD_PRIORITY, 2);
    assert(transaction_thread_id);
    rt_thread_startup(transaction_thread_id);

    /* Thread - RX processing */
    process_rx_thread_id = rt_thread_create("esp_spi_rx", process_rx_thread, NULL, ESP_HOSTED_SPI_THREAD_STACK_SIZE, ESP_HOSTED_SPI_THREAD_PRIORITY, 2);
    assert(process_rx_thread_id);
    rt_thread_startup(process_rx_thread_id);

    /* Initializes an external interrupt */
    rt_pin_mode(ESP_HOSTED_HANDSHAKE_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_mode(ESP_HOSTED_DATA_READY_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_attach_irq(ESP_HOSTED_HANDSHAKE_PIN, PIN_IRQ_MODE_RISING, gpio_interrupt, NULL);
    rt_pin_attach_irq(ESP_HOSTED_DATA_READY_PIN, PIN_IRQ_MODE_RISING, gpio_interrupt, NULL);
    rt_pin_irq_enable(ESP_HOSTED_HANDSHAKE_PIN, RT_TRUE);
    rt_pin_irq_enable(ESP_HOSTED_DATA_READY_PIN, RT_TRUE);
}

/**
  * @brief  Send to slave via SPI
  * @param  iface_type -type of interface
  *         iface_num - interface number
  *         wbuffer - tx buffer
  *         wlen - size of wbuffer
  * @retval sendbuf - Tx buffer
  */
esp_ret send_to_slave(uint8_t iface_type, uint8_t iface_num,
        uint8_t * wbuffer, uint16_t wlen)
{
    interface_buffer_handle_t buf_handle = {0};

    if (!wbuffer || !wlen || (wlen > MAX_PAYLOAD_SIZE))
    {
        LOG_E("write fail: buff(%p) 0? OR (0<len(%u)<=max_poss_len(%u))?",
                wbuffer, wlen, MAX_PAYLOAD_SIZE);
        if(wbuffer) {
            free(wbuffer);
            wbuffer = NULL;
        }
        return ESP_FAIL;
    }
    memset(&buf_handle, 0, sizeof(buf_handle));

    buf_handle.if_type = iface_type;
    buf_handle.if_num = iface_num;
    buf_handle.payload_len = wlen;
    buf_handle.payload = wbuffer;
    buf_handle.priv_buffer_handle = wbuffer;
    buf_handle.free_buf_handle = free;

    if (RT_EOK != rt_mq_send_wait(to_slave_queue, &buf_handle, sizeof(buf_handle), RT_WAITING_FOREVER))
    {
        LOG_E("Failed to send tx buffer to the queue");
        if(wbuffer) {
            free(wbuffer);
            wbuffer = NULL;
        }
        return ESP_FAIL;
    }

    rt_sem_release(trans_semaphore);

    return ESP_OK;
}

/**
  * @brief  Full duplex transaction SPI transaction for ESP32S2 hardware
  * @param  txbuff: TX SPI buffer
  * @retval ESP_OK / ESP_FAIL
  */
static esp_ret spi_transaction(struct rt_spi_device *dev, uint8_t * txbuff)
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

    if(!txbuff)
    {
        /* Even though, there is nothing to send,
         * valid reseted txbuff is needed for SPI driver
         */
        txbuff = (uint8_t *)malloc(MAX_SPI_BUFFER_SIZE);
        assert(txbuff);
        memset(txbuff, 0, MAX_SPI_BUFFER_SIZE);
    }

    /* SPI transaction */
    rx_length = rt_spi_transfer(dev, txbuff, rxbuff, MAX_SPI_BUFFER_SIZE);

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
            if (payload_header->if_num == 0xF && payload_header->if_type == ESP_MAX_IF)
            {
                /* dummy packet, ignore it */
                LOG_D("Dummy packet received from slave");
            }
            else
            {
                LOG_W("Invalid packet received from slave, length: %d", rx_length);
                LOG_HEX("rxbuff_head16", 16, rxbuff, min(rx_length, 32));
                LOG_HEX("rxbuff_tail16", 16, rxbuff + (rx_length - min(rx_length, 32)), min(rx_length, 32));
            }

            /* Free up buffer, as one of following -
                * 1. no payload to process
                * 2. input packet size > driver capacity
                * 3. payload header size mismatch,
                * wrong header/bit packing?
                * */
            if (rxbuff)
            {
                free(rxbuff);
                rxbuff = NULL;
            }
        }
        else
        {
            rx_checksum = le16toh(payload_header->checksum);
            payload_header->checksum = 0;

            checksum = compute_checksum(rxbuff, len+offset);

            if (checksum == rx_checksum)
            {
                buf_handle.priv_buffer_handle = rxbuff;
                buf_handle.free_buf_handle = free;
                buf_handle.payload_len = len;
                buf_handle.if_type     = payload_header->if_type;
                buf_handle.if_num      = payload_header->if_num;
                buf_handle.payload     = rxbuff + offset;
                buf_handle.seq_num     = le16toh(payload_header->seq_num);
                buf_handle.flag        = payload_header->flags;
                LOG_D("if_type: %d, if_num: %d, len: %d, offset: %d, seq_num: %d, flags: 0x%x",
                        payload_header->if_type, payload_header->if_num, len, offset, le16toh(payload_header->seq_num), payload_header->flags);
                LOG_HEX("payload", 16, buf_handle.payload, min(buf_handle.payload_len, 32));
                if (RT_EOK != rt_mq_send_wait(from_slave_queue, &buf_handle, sizeof(interface_buffer_handle_t), 2000))
                {
                    LOG_E("Failed to send rx buffer to the queue");
                    goto done;
                }
            }
            else
            {
                LOG_W("Checksum mismatch, rx_checksum: 0x%04x, computed_checksum: 0x%04x", rx_checksum, checksum);
                if (rxbuff) {
                    free(rxbuff);
                    rxbuff = NULL;
                }
            }
        }

        /* Free input TX buffer */
        if (txbuff)
        {
            free(txbuff);
            txbuff = NULL;
        }
    }
    else
    {
        goto done;
    }

    return ESP_OK;

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
    return ESP_FAIL;
}

/**
  * @brief  Thread for SPI transaction
  * @param  argument: Not used
  * @retval None
  */
static void transaction_thread(void *parameter)
{
    uint8_t * txbuff = NULL;
    uint8_t is_valid_tx_buf;
    rt_bool_t gpio_handshake;
    rt_bool_t gpio_rx_data_ready;
    struct rt_spi_device *spi_device = (struct rt_spi_device *)parameter;

    while(1)
    {
        /* Wait till slave is ready for next transaction */
        rt_sem_take(trans_semaphore, RT_WAITING_FOREVER);

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
                spi_transaction(spi_device, txbuff);
            }
        }
    }
}

/**
  * @brief  RX processing thread
  * @param  argument: Not used
  * @retval None
  */
static void process_rx_thread(void *parameter)
{
    interface_buffer_handle_t buf_handle = {0};
    uint8_t *payload = NULL;
    struct pbuf *buffer = NULL;
    struct esp_priv_event *event = NULL;
    struct esp_private *priv = NULL;

    LOG_D("Starting RX processing thread");

    while (1)
    {
        if (rt_mq_recv(from_slave_queue, &buf_handle, sizeof(interface_buffer_handle_t), RT_WAITING_FOREVER) != sizeof(interface_buffer_handle_t))
        {
            LOG_E("Failed to receive rx buffer in the queue");
            continue;
        }

        /* point to payload */
        payload = buf_handle.payload;

        /* process received buffer for all possible interface types */
        if (buf_handle.if_type == ESP_SERIAL_IF)
        {
            /* serial interface path */
            serial_rx_handler(&buf_handle);
        }
        else if ((buf_handle.if_type == ESP_STA_IF) || (buf_handle.if_type == ESP_AP_IF))
        {
            priv = get_priv(buf_handle.if_type, buf_handle.if_num);

            if (priv)
            {
                buffer = (struct pbuf *)malloc(sizeof(struct pbuf));
                assert(buffer);

                buffer->len = buf_handle.payload_len;
                buffer->payload = malloc(buf_handle.payload_len);
                assert(buffer->payload);

                memcpy(buffer->payload, buf_handle.payload,
                        buf_handle.payload_len);

                netdev_stub_rx(priv->netdev, buffer);
            }
        }
        else if (buf_handle.if_type == ESP_PRIV_IF)
        {
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
            if (event->event_type == ESP_PRIV_EVENT_INIT)
            {
                /* halt spi transactions for some time,
                 * this is one time delay, to give breathing
                 * time to slave before spi trans start */
                // rt_thread_mdelay(100);
                if (spi_drv_evt_handler_fp) {
                    spi_drv_evt_handler_fp(TRANSPORT_ACTIVE);
                }
            }
            else
            {
                /* User can re-use this type of transaction */
            }
        }
        else if (buf_handle.if_type == ESP_TEST_IF)
        {
#if TEST_RAW_TP
            update_test_raw_tp_rx_len(buf_handle.payload_len);
#endif
        }
        else
        {
            LOG_E("Received unknown interface type %d ", buf_handle.if_type);
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
    if (rt_mq_recv(to_slave_queue, &buf_handle, sizeof(buf_handle), 0) == sizeof(buf_handle)) {
        len = buf_handle.payload_len;
    }

    if (len)
    {

        sendbuf = (uint8_t *) malloc(MAX_SPI_BUFFER_SIZE);
        if (!sendbuf)
        {
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
