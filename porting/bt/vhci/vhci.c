/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-01-19     Evlers       first version.
 */

#include <string.h>
#include <stdlib.h>
#include <rtthread.h>
#include <drivers/vhci.h>

static rt_err_t rt_vhci_init(struct rt_device *dev)
{
    rt_vhci_dev_t *vhci_core;

    RT_ASSERT(dev != RT_NULL);
    vhci_core = (rt_vhci_dev_t *)dev;

    if (vhci_core->ops->init)
    {
        vhci_core->lock = rt_mutex_create("vhci", RT_IPC_FLAG_FIFO);
        return (vhci_core->ops->init());
    }

    return -RT_ENOSYS;
}

static rt_err_t rt_vhci_open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_vhci_dev_t *vhci_core;

    RT_ASSERT(dev != RT_NULL);
    vhci_core = (rt_vhci_dev_t *)dev;

    if (vhci_core->ops->open)
    {
        rt_err_t ret;
        rt_mutex_take(vhci_core->lock, RT_WAITING_FOREVER);
        ret = (vhci_core->ops->open(dev, oflag));
        rt_mutex_release(vhci_core->lock);
        return ret;
    }

    return -RT_ENOSYS;
}

static rt_err_t rt_vhci_close(struct rt_device *dev)
{
    rt_vhci_dev_t *vhci_core;

    RT_ASSERT(dev != RT_NULL);
    vhci_core = (rt_vhci_dev_t *)dev;

    if (vhci_core->ops->close)
    {
        rt_err_t ret;
        rt_mutex_take(vhci_core->lock, RT_WAITING_FOREVER);
        ret = (vhci_core->ops->close(dev));
        rt_mutex_release(vhci_core->lock);
        return ret;
    }

    return -RT_ENOSYS;
}

static rt_ssize_t rt_vhci_read(struct rt_device *dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_vhci_dev_t *vhci_core;

    RT_ASSERT(dev != RT_NULL);
    vhci_core = (rt_vhci_dev_t *)dev;

    if (vhci_core->ops->read)
    {
        rt_err_t ret;
        rt_mutex_take(vhci_core->lock, RT_WAITING_FOREVER);
        ret = (vhci_core->ops->read(dev, pos, buffer, size));
        rt_mutex_release(vhci_core->lock);
        return ret;
    }

    return -RT_ENOSYS;
}

static rt_ssize_t rt_vhci_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    rt_vhci_dev_t *vhci_core;

    RT_ASSERT(dev != RT_NULL);
    vhci_core = (rt_vhci_dev_t *)dev;

    if (vhci_core->ops->write)
    {
        rt_err_t ret;
        rt_mutex_take(vhci_core->lock, RT_WAITING_FOREVER);
        ret = (vhci_core->ops->write(dev, pos, buffer, size));
        rt_mutex_release(vhci_core->lock);
        return ret;
    }

    return -RT_ENOSYS;
}

static rt_err_t rt_vhci_control(struct rt_device *dev, int cmd, void *args)
{
    return RT_EOK;
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops vhci_core_ops =
{
    rt_vhci_init,
    rt_vhci_open,
    rt_vhci_close,
    rt_vhci_read,
    rt_vhci_write,
    rt_vhci_control,
};
#endif /* RT_USING_DEVICE_OPS */

rt_err_t rt_device_vhci_register(rt_vhci_dev_t *vhci, const char *name, rt_uint32_t flag, void *user_data)
{
    struct rt_device *device;
    RT_ASSERT(vhci != RT_NULL);

    device = &(vhci->parent);

    device->type        = RT_Device_Class_Char;
    device->rx_indicate = RT_NULL;
    device->tx_complete = RT_NULL;

#ifdef RT_USING_DEVICE_OPS
    device->ops         = &vhci_core_ops;
#else
    device->init        = rt_vhci_init;
    device->open        = rt_vhci_open;
    device->close       = rt_vhci_close;
    device->read        = rt_vhci_read;
    device->write       = rt_vhci_write;
    device->control     = rt_vhci_control;
#endif /* RT_USING_DEVICE_OPS */
    device->user_data   = user_data;

    /* register a character device */
    return rt_device_register(device, name, flag);
}