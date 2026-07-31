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
 * 2024-12-26   Evlers      first implementation
 */

#include "os_wrapper.h"
#include "transport_drv.h"
#include "spi_wrapper.h"

#include "esp_log.h"
#include "transport_drv.h"
#include "rtdevice.h"

extern void *spi_handle;

rt_weak void *hosted_spi_init(void)
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
        struct rt_spi_configuration cfg = {0};
        cfg.data_width = 8;
        cfg.mode = ESP_HOSTED_SPI_MODE | RT_SPI_MSB;
        cfg.max_hz = ESP_HOSTED_SPI_MAX_HZ;
        if (rt_spi_configure(&esp_spi_device, &cfg) != RT_EOK)
        {
            ESP_LOGE("Failed to configure spi device (%s)", ESP_HOSTED_SPI_DEVICE_NAME);
            return NULL;
        }
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
