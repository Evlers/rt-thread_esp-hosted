/*
 * Copyright (c) 2006-2024, Evlers Developers
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date         Author      Notes
 * 2024-01-11   Evlers      first implementation
 */

#include <rtthread.h>

#if defined(RT_USING_ESP_HOSTED) && defined(ESP_HOSTED_USING_SAMPLE)
#include <rtdevice.h>
#include <drv_spi.h>
#include <board.h>


extern int rt_hw_esp_wlan_init (void);

static void esp_hosted_init (void *parameter)
{
    /* reset esp32 chip */
    rt_pin_mode(ESP_HOSTED_RESET_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(ESP_HOSTED_RESET_PIN, PIN_LOW);
    rt_thread_mdelay(50);
    rt_pin_write(ESP_HOSTED_RESET_PIN, PIN_HIGH);
    
    /* stop spi transactions short time to avoid slave sync issues */
	rt_thread_mdelay(50);

    /* attach spi device */
    rt_hw_spi_device_attach(ESP_HOSTED_SPI_BUS_NAME, ESP_HOSTED_SPI_DEVICE_NAME, ESP_HOSTED_SPI_CS_PIN);

    /* Initialize the esp-hosted */
    rt_hw_esp_wlan_init();
}

int esp_spi_device_init (void)
{
#ifdef ESP_HOSTED_THREAD_INIT
    /* Use thread initialization */
    rt_thread_t init_thread = rt_thread_create("esp_init", esp_hosted_init, NULL, 
                                                ESP_HOSTED_INIT_THREAD_STACK_SIZE, ESP_HOSTED_INIT_THREAD_PRIORITY, 20);
    RT_ASSERT(init_thread != NULL);
    rt_thread_startup(init_thread);
#else
    /* Thread initialization is not used */
    esp_hosted_init(NULL);
#endif

    return RT_EOK;
}
INIT_APP_EXPORT(esp_spi_device_init);


#endif /* defined(RT_USING_ESP_HOSTED) && defined(ESP_HOSTED_USING_SAMPLE) */
