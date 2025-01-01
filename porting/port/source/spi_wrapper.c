/*
 * Copyright (c) 2006-2024 Evlers Developers
 *
 * Change Logs:
 * Date         Author      Notes
 * 2024-12-26   Evlers      first implementation
 */

#include "os_wrapper.h"
#include "transport_drv.h"
#include "spi_wrapper.h"

#include "esp_log.h"
#include "transport_drv.h"
#include "rtdevice.h"

DEFINE_LOG_TAG(spi_wrapper);

extern void * spi_handle;

void *hosted_spi_init(void)
{
    /* Initializes the spi bus */
#ifdef ESP_HOSTED_USING_PIN_NUMBER
    rt_base_t pin_cs = ESP_HOSTED_SPI_CS_PIN;
#else
    rt_base_t pin_cs = rt_pin_get(ESP_HOSTED_SPI_CS_PIN_NAME);
#endif
    static struct rt_spi_device esp_spi_device;
    if (rt_spi_bus_attach_device_cspin(&esp_spi_device, ESP_HOSTED_SPI_DEVICE_NAME, ESP_HOSTED_SPI_BUS_NAME, pin_cs, NULL) == RT_EOK)
    {
        /* Configure SPI bus */
        struct rt_spi_configuration cfg;
        cfg.data_width = 8;
        cfg.mode = RT_SPI_MODE_3 | RT_SPI_MSB;
        cfg.max_hz = ESP_HOSTED_SPI_MAX_HZ;
        rt_spi_configure(&esp_spi_device, &cfg);
    }
    else
    {
        ESP_LOGE("No spi device (%s) is found. Please attach the spi bus!", ESP_HOSTED_SPI_DEVICE_NAME);
        return NULL;
    }

    return &esp_spi_device;
}

int hosted_do_spi_transfer(void *trans)
{
    struct rt_spi_device *dev = spi_handle;
    struct hosted_transport_context_t *spi_trans = trans;

    if (rt_spi_transfer(dev, spi_trans->tx_buf, spi_trans->rx_buf, spi_trans->tx_buf_size) != spi_trans->tx_buf_size)
    {
        return RET_FAIL;
    }
    
    return RET_OK;
}
