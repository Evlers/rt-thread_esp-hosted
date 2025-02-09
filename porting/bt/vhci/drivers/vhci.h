/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-01-19     Evlers       first version.
 */

#ifndef _VHCI_H_
#define _VHCI_H_

#include <rtdef.h>
#include <ipc/ringbuffer.h>

struct rt_vhci_ops
{
    rt_err_t (*init)(void);
    rt_err_t  (*open)   (rt_device_t dev, rt_uint16_t oflag);
    rt_err_t  (*close)  (rt_device_t dev);
    rt_ssize_t (*read)  (rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size);
    rt_ssize_t (*write) (rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size);
};

typedef struct rt_vhci_device
{
    struct rt_device parent;
    const struct rt_vhci_ops *ops;
    rt_mutex_t lock;
    struct rt_ringbuffer *rb;
} rt_vhci_dev_t;

rt_err_t rt_device_vhci_register(rt_vhci_dev_t *vhci, const char *name, rt_uint32_t flag, void *user_data);

#endif /* _VHCI_H_ */
