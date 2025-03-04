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
 * 2025-01-19   Evlers      first implementation
 */

#include <stdint.h>
#include "rtthread.h"
#include "rtdevice.h"
#include "ipc/ringbuffer.h"
#include "adapter.h"
#include "os_wrapper.h"
#include "transport_drv.h"
#include "hci_drv.h"
#include "esp_hosted_log.h"
#include "drivers/vhci.h"

static const char TAG[] = "vhci.dev";


static rt_err_t rt_vhci_init(void)
{
    return RT_EOK;
}

static rt_err_t rt_vhci_open(rt_device_t dev, rt_uint16_t oflag)
{
    ((rt_vhci_dev_t *)dev)->rb = rt_ringbuffer_create(ESP_HOSTED_VHCI_DEVICE_BUF_SIZE);
    if (((rt_vhci_dev_t *)dev)->rb == RT_NULL)
    {
        ESP_LOGE(TAG, "vhci ringbuffer create failed");
        return -RT_ERROR;
    }
    return RT_EOK;
}

static rt_err_t rt_vhci_close(rt_device_t dev)
{
    return RT_EOK;
}

static rt_ssize_t rt_vhci_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_size_t read_size = 0;
    rt_uint8_t *data = (rt_uint8_t *)buffer;
    rt_size_t data_len = 0;

    if (((rt_vhci_dev_t *)dev)->rb == RT_NULL)
    {
        ESP_LOGE(TAG, "vhci ringbuffer is null");
        return -RT_ERROR;
    }

    read_size = rt_ringbuffer_data_len(((rt_vhci_dev_t *)dev)->rb);
    if (read_size == 0)
    {
        return 0;
    }

    data_len = read_size > size ? size : read_size;
    rt_ringbuffer_get(((rt_vhci_dev_t *)dev)->rb, data, data_len);
    return data_len;
}

static rt_ssize_t rt_vhci_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    uint8_t *data = g_h.funcs->_h_malloc(size);

    if (!data)
    {
        ESP_LOGE(TAG, "Tx %s: malloc failed", __func__);
        return -RT_ERROR;
    }

    memcpy(data, buffer, size);
    if (esp_hosted_tx(ESP_HCI_IF, 0, data, size, H_BUFF_NO_ZEROCOPY, H_DEFLT_FREE_FUNC) != ESP_OK)
    {
        return -RT_ERROR;
    }

    return size;
}

static struct rt_vhci_ops vhci_ops =
{
    .init = rt_vhci_init,
    .open = rt_vhci_open,
    .close = rt_vhci_close,
    .read = rt_vhci_read,
    .write = rt_vhci_write
};

static rt_vhci_dev_t vhci_dev =
{
    .ops = &vhci_ops
};

static int rt_vhci_dev_init(void)
{
    /* register virtual hci device */
    rt_device_vhci_register(&vhci_dev, ESP_HOSTED_VHCI_DEVICE_NAME, RT_DEVICE_FLAG_RDWR, NULL);
    return RT_EOK;
}
INIT_DEVICE_EXPORT(rt_vhci_dev_init);


/* ESP-Hosted HCI interface */

void hci_drv_init(void)
{
    /* do nothing for VHCI: underlying transport should be ready */
}

rt_weak void esp_hosted_bt_startup (void)
{
    /* The callback indicates that the vhci interface is ready.
     * The user rewrites this function to initialize or configure the Bluetooth protocol stack.
     */
}

void hci_drv_show_configuration(void)
{
	ESP_LOGI(TAG, "Host BT Support: Enabled");
	ESP_LOGI(TAG, "BT Transport Type: vhci devices");
    esp_hosted_bt_startup();
}

int hci_rx_handler(interface_buffer_handle_t *buf_handle)
{
    rt_size_t rx_length = 0;

    if (rt_ringbuffer_put(vhci_dev.rb, buf_handle->payload, buf_handle->payload_len) != buf_handle->payload_len)
    {
        ESP_LOGE(TAG, "vhci ringbuffer put failed");
        return ESP_ERR_NO_MEM;
    }

    /* get the length of the data from the ringbuffer */
    rx_length = rt_ringbuffer_data_len(vhci_dev.rb);
    if (rx_length)
    {
        /* trigger the receiving completion callback */
        if (vhci_dev.parent.rx_indicate != RT_NULL)
        {
            vhci_dev.parent.rx_indicate(&(vhci_dev.parent), rx_length);
        }
    }

	return ESP_OK;
}
