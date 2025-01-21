/*
 * Copyright (c) 2006-2025 Evlers Developers
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

static struct rt_ringbuffer *vhci_rx_buffer = RT_NULL;

void hci_drv_init(void)
{
    /* do nothing for VHCI: underlying transport should be ready */
}

void hci_drv_show_configuration(void)
{
	ESP_LOGI(TAG, "Host BT Support: Enabled");
	ESP_LOGI(TAG, "BT Transport Type: vhci devices");
}

int hci_rx_handler(interface_buffer_handle_t *buf_handle)
{
    if (rt_ringbuffer_put(vhci_rx_buffer, buf_handle->payload, buf_handle->payload_len) != buf_handle->payload_len)
    {
        ESP_LOGE(TAG, "vhci_rx_buffer put failed");
        return ESP_ERR_NO_MEM;
    }
	return ESP_OK;
}

static rt_err_t rt_vhci_init(void)
{
    return RT_EOK;
}

static rt_err_t rt_vhci_open(rt_device_t dev, rt_uint16_t oflag)
{
    vhci_rx_buffer = rt_ringbuffer_create(1024);
    if (vhci_rx_buffer == RT_NULL)
    {
        ESP_LOGE(TAG, "vhci_rx_buffer create failed");
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

    if (vhci_rx_buffer == RT_NULL)
    {
        ESP_LOGE(TAG, "vhci_rx_buffer is null");
        return -RT_ERROR;
    }

    read_size = rt_ringbuffer_data_len(vhci_rx_buffer);
    if (read_size == 0)
    {
        return 0;
    }

    data_len = read_size > size ? size : read_size;
    rt_ringbuffer_get(vhci_rx_buffer, data, data_len);
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
