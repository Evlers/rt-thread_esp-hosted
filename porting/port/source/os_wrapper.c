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
 * 2025-01-01   Evlers      first implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "esp_log_level.h"
#include "esp_hosted_config.h"
#include "esp_hosted_log.h"
#include "os_wrapper.h"
#include "rtdbg.h"

/* Wi-Fi headers are reused at ESP-Hosted */
#include "esp_wifi_crypto_types.h"
// #include "esp_private/wifi_os_adapter.h"

#ifdef CONFIG_ESP_SDIO_HOST_INTERFACE
#include "sdio_wrapper.h"
#endif

#ifdef CONFIG_ESP_SPI_HOST_INTERFACE
#include "spi_wrapper.h"
#endif

#ifdef CONFIG_ESP_SPI_HD_HOST_INTERFACE
#include "spi_hd_wrapper.h"
#endif

#ifdef CONFIG_ESP_UART_HOST_INTERFACE
#include "uart_wrapper.h"
#endif

DEFINE_LOG_TAG(os_wrapper_esp);

struct hosted_config_t g_h = HOSTED_CONFIG_INIT_DEFAULT();

/* -------- Memory ---------- */

void *hosted_memcpy(void* dest, const void* src, uint32_t size)
{
	if (size && (!dest || !src)) {
		if (!dest)
			ESP_LOGE(TAG, "%s:%u dest is NULL\n", __func__, __LINE__);
		if (!src)
			ESP_LOGE(TAG, "%s:%u dest is NULL\n", __func__, __LINE__);

		assert(dest);
		assert(src);
		return NULL;
	}

	return memcpy(dest, src, size);
}

void *hosted_memset(void* buf, int val, size_t len)
{
	return memset(buf, val, len);
}

void *hosted_malloc(size_t size)
{
	return rt_malloc(size);
}

void* hosted_calloc(size_t blk_no, size_t size)
{
	return rt_calloc(blk_no, size);
}

void hosted_free(void* ptr)
{
	rt_free(ptr);
}

void *hosted_realloc(void *mem, size_t newsize)
{
	void *p = NULL;

	if (newsize == 0) {
		HOSTED_FREE(mem);
		return NULL;
	}

	p = hosted_malloc(newsize);
	if (p) {
		/* zero the memory */
		if (mem != NULL) {
			hosted_memcpy(p, mem, newsize);
			HOSTED_FREE(mem);
		}
	}
	return p;
}

void *hosted_malloc_align(size_t size, size_t align)
{
	return rt_malloc_align(size, align);
}

void hosted_free_align(void* ptr)
{
	rt_free_align(ptr);
}


void hosted_init_hook(void)
{
	/* This is hook to initialize port specific contexts, if any */
}


/* -------- Threads ---------- */
void *hosted_thread_create(char *tname, uint32_t tprio, uint32_t tstack_size, void (*start_routine)(void const *), void *sr_arg)
{
	if (!start_routine) {
		ESP_LOGE(TAG, "start_routine is mandatory for thread create\n");
		return NULL;
	}

	thread_handle_t thread = rt_thread_create(tname, (void (*)(void *))start_routine, sr_arg, tstack_size, tprio, 10);

	if (!thread) {
        ESP_LOGE(TAG, "Failed to allocate thread handle");
        return NULL;
    }

    if (rt_thread_startup(thread) != RT_EOK) {
        ESP_LOGE(TAG, "Failed to create ctrl path task");
        rt_thread_delete(thread);
        return NULL;
    }

	return thread;
}

int hosted_thread_cancel(void *thread_handle)
{
	return rt_thread_suspend(thread_handle);
}

/* -------- Sleeps -------------- */
unsigned int hosted_msleep(unsigned int mseconds)
{
	rt_thread_mdelay(mseconds);
   	return 0;
}

unsigned int hosted_usleep(unsigned int useconds)
{
	rt_thread_delay(useconds);
   	return 0;
}

unsigned int hosted_sleep(unsigned int seconds)
{
   return hosted_msleep(seconds * 1000UL);
}

/* Non sleepable delays - BLOCKING dead wait */
unsigned int hosted_for_loop_delay(unsigned int number)
{
	volatile int idx = 0;
	for (idx=0; idx<100*number; idx++) {
	}
	return 0;
}



/* -------- Queue --------------- */
void * hosted_create_queue(uint32_t qnum_elem, uint32_t qitem_size)
{
	return rt_mq_create("esp-hosted", qitem_size, qnum_elem, RT_IPC_FLAG_PRIO);
}

int hosted_queue_item(void * queue_handle, void *item, int timeout)
{
	queue_handle_t mq = queue_handle;
	return rt_mq_send_wait((queue_handle_t)queue_handle, item, mq->msg_size, rt_tick_from_millisecond(timeout) * 1000);
}

int hosted_dequeue_item(void * queue_handle, void *item, int timeout)
{
	queue_handle_t mq = queue_handle;
	return (rt_mq_recv(mq, item, mq->msg_size, rt_tick_from_millisecond(timeout) * 1000) <= 0) ? RET_FAIL_TIMEOUT : RET_OK;
}

int hosted_queue_msg_waiting(void * queue_handle)
{
	return RET_FAIL;
}

int hosted_destroy_queue(void * queue_handle)
{
	return rt_mq_delete((queue_handle_t)queue_handle);
}

int hosted_reset_queue(void * queue_handle)
{
	return rt_mq_control((queue_handle_t)queue_handle, RT_IPC_CMD_RESET, NULL);
}

/* -------- Mutex --------------- */
void * hosted_create_mutex(void)
{
	return rt_mutex_create("esp_hosted", RT_IPC_FLAG_PRIO);
}

int hosted_unlock_mutex(void * mutex_handle)
{
	return rt_mutex_release((mutex_handle_t)mutex_handle);
}

int hosted_lock_mutex(void * mutex_handle, int timeout)
{
	return rt_mutex_take((mutex_handle_t)mutex_handle, rt_tick_from_millisecond(timeout) * 1000);
}

int hosted_destroy_mutex(void * mutex_handle)
{
	return rt_mutex_delete((mutex_handle_t)mutex_handle);
}

/* -------- Semaphores ---------- */
void * hosted_create_semaphore(int maxCount)
{
	semaphore_handle_t semaphore_handle = rt_sem_create("esp-hosted", 0, RT_IPC_FLAG_PRIO);

	if (semaphore_handle != NULL)
	{
		rt_sem_release(semaphore_handle);
	}

	return semaphore_handle;
}

int hosted_post_semaphore(void * semaphore_handle)
{
	return rt_sem_release((semaphore_handle_t)semaphore_handle);
}

FAST_RAM_ATTR int hosted_post_semaphore_from_isr(void * semaphore_handle)
{
	return rt_sem_release((semaphore_handle_t)semaphore_handle);
}

int hosted_get_semaphore(void * semaphore_handle, int timeout)
{
	return rt_sem_take((semaphore_handle_t)semaphore_handle, rt_tick_from_millisecond(timeout) * 1000);
}

int hosted_destroy_semaphore(void * semaphore_handle)
{
	return rt_sem_delete((semaphore_handle_t)semaphore_handle);
}

#ifdef CONFIG_USE_MEMPOOL
static void * hosted_create_spinlock(void)
{
	RT_DEFINE_SPINLOCK(spin_dummy);
	spinlock_handle_t *spin_id = NULL;

	spin_id = (spinlock_handle_t*)hosted_malloc(
			sizeof(spinlock_handle_t));

	if (!spin_id) {
		ESP_LOGE(TAG, "mut allocation failed\n");
		return NULL;
	}

	spin_id->lock = spin_dummy;
	rt_spin_lock_init(spin_id);

	return spin_id;
}

void* hosted_create_lock_mempool(void)
{
	return hosted_create_spinlock();
}
void hosted_lock_mempool(void *lock_handle)
{
	assert(lock_handle);
	rt_spin_lock((spinlock_handle_t)lock_handle);
}

void hosted_unlock_mempool(void *lock_handle)
{
	assert(lock_handle);
	rt_spin_unlock((spinlock_handle_t)lock_handle);
}
#endif

/* -------- Timers  ---------- */
void *hosted_timer_start(int duration, int type, void (*timeout_handler)(void *), void *arg)
{
	rt_timer_t timer_handle = rt_timer_create("esp-hosted", (void (*)(void *))timeout_handler, arg,
                            					rt_tick_from_millisecond(duration) * 1000,
                            					type);

    if (timer_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate timer handle");
        return NULL;
    }

    if (rt_timer_start(timer_handle) != RT_EOK)
    {
        ESP_LOGE(TAG, "Failed to start timer, destroying timer");
        rt_timer_delete(timer_handle);
        return NULL;
    }

    return timer_handle;
}

int hosted_timer_stop(void *timer_handle)
{
	if (!timer_handle) {
        ESP_LOGE(TAG, "Uninitialized timer handle");
        return ESP_FAIL;
    }

    if (rt_timer_stop(timer_handle) != RT_EOK)
    {
        ESP_LOGE(TAG, "Unable to stop timer");
        return ESP_FAIL;
    }

    if (rt_timer_delete(timer_handle) != RT_EOK)
    {
        ESP_LOGE(TAG, "Unable to delete timer");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* GPIO */

int hosted_config_gpio(void* gpio_port, uint32_t gpio_num, uint32_t mode)
{
	rt_pin_mode(gpio_num, mode);
}

int hosted_config_gpio_as_interrupt(void* gpio_port, uint32_t gpio_num, uint32_t intr_type, void (*new_gpio_isr_handler)(void* arg))
{
	rt_pin_mode(gpio_num, PIN_MODE_INPUT);
	rt_pin_attach_irq(gpio_num, intr_type, new_gpio_isr_handler, NULL);
	rt_pin_irq_enable(gpio_num, RT_TRUE);
	return 0;
}

int hosted_read_gpio(void*gpio_port, uint32_t gpio_num)
{
	return rt_pin_read(gpio_num);
}

int hosted_write_gpio(void* gpio_port, uint32_t gpio_num, uint32_t value)
{
	rt_pin_write(gpio_num, value);
	return 0;
}

rt_weak int hosted_wifi_event_post(int32_t event_id, void* event_data, size_t event_data_size, uint32_t ticks_to_wait)
{
	ESP_LOGI(TAG, "wifi event: %d", event_id);
	return 0;
}

void hosted_log_write(int level, const char *tag, const char *format, ...)
{
	va_list args;
    rt_size_t length = 0;
    static char rt_log_buf[RT_CONSOLEBUF_SIZE];

	esp_log_level_t level_for_tag = esp_log_level_get(tag);
	if ((ESP_LOG_NONE != level_for_tag) && (level <= level_for_tag))
	{
		va_start(args, format);
		/* the return value of vsnprintf is the number of bytes that would be
		* written to buffer had if the size of the buffer been sufficiently
		* large excluding the terminating null byte. If the output string
		* would be larger than the rt_log_buf, we have to adjust the output
		* length. */
		length = rt_vsnprintf(rt_log_buf, sizeof(rt_log_buf) - 1, format, args);
		if (length > RT_CONSOLEBUF_SIZE - 1)
		{
			length = RT_CONSOLEBUF_SIZE - 1;
		}

		#undef LOG_TAG
		#define LOG_TAG tag
		switch (level)
		{
			default: 				LOG_D("%.*s", length, rt_log_buf); break;
			case ESP_LOG_INFO: 		LOG_I("%.*s", length, rt_log_buf); break;
			case ESP_LOG_WARN: 		LOG_W("%.*s", length, rt_log_buf); break;
			case ESP_LOG_ERROR: 	LOG_E("%.*s", length, rt_log_buf); break;
		}
		#undef LOG_TAG

		va_end(args);
	}
}

/* newlib hooks */

hosted_osi_funcs_t g_hosted_osi_funcs = {
	._h_memcpy                   =  hosted_memcpy                  ,
	._h_memset                   =  hosted_memset                  ,
	._h_malloc                   =  hosted_malloc                  ,
	._h_calloc                   =  hosted_calloc                  ,
	._h_free                     =  hosted_free                    ,
	._h_realloc                  =  hosted_realloc                 ,
	._h_malloc_align             =  hosted_malloc_align            ,
	._h_free_align               =  hosted_free_align              ,
	._h_thread_create            =  hosted_thread_create           ,
	._h_thread_cancel            =  hosted_thread_cancel           ,
	._h_msleep                   =  hosted_msleep                  ,
	._h_usleep                   =  hosted_usleep                  ,
	._h_sleep                    =  hosted_sleep                   ,
	._h_blocking_delay           =  hosted_for_loop_delay          ,
	._h_queue_item               =  hosted_queue_item              ,
	._h_create_queue             =  hosted_create_queue            ,
	._h_queue_msg_waiting        =  hosted_queue_msg_waiting       ,
	._h_dequeue_item             =  hosted_dequeue_item            ,
	._h_destroy_queue            =  hosted_destroy_queue           ,
	._h_reset_queue              =  hosted_reset_queue             ,
	._h_unlock_mutex             =  hosted_unlock_mutex            ,
	._h_create_mutex             =  hosted_create_mutex            ,
	._h_lock_mutex               =  hosted_lock_mutex              ,
	._h_destroy_mutex            =  hosted_destroy_mutex           ,
	._h_post_semaphore           =  hosted_post_semaphore          ,
	._h_post_semaphore_from_isr  =  hosted_post_semaphore_from_isr ,
	._h_create_semaphore         =  hosted_create_semaphore        ,
	._h_get_semaphore            =  hosted_get_semaphore           ,
	._h_destroy_semaphore        =  hosted_destroy_semaphore       ,
	._h_timer_stop               =  hosted_timer_stop              ,
	._h_timer_start              =  hosted_timer_start             ,
#ifdef CONFIG_USE_MEMPOOL
	._h_create_lock_mempool      =  hosted_create_lock_mempool     ,
	._h_lock_mempool             =  hosted_lock_mempool            ,
	._h_unlock_mempool           =  hosted_unlock_mempool          ,
#endif
	._h_config_gpio              =  hosted_config_gpio             ,
	._h_config_gpio_as_interrupt =  hosted_config_gpio_as_interrupt,
	._h_read_gpio                =  hosted_read_gpio               ,
	._h_write_gpio               =  hosted_write_gpio              ,
#ifdef CONFIG_ESP_SPI_HOST_INTERFACE
	._h_bus_init                 =  hosted_spi_init                ,
	._h_do_bus_transfer          =  hosted_do_spi_transfer         ,
#endif
	._h_event_wifi_post          =  hosted_wifi_event_post         ,
	._h_printf                   =  hosted_log_write               ,
	._h_hosted_init_hook         =  hosted_init_hook               ,
#ifdef CONFIG_ESP_SDIO_HOST_INTERFACE
	._h_bus_init                 =  hosted_sdio_init               ,
	._h_sdio_card_init           =  hosted_sdio_card_init          ,
	._h_sdio_read_reg            =  hosted_sdio_read_reg           ,
	._h_sdio_write_reg           =  hosted_sdio_write_reg          ,
	._h_sdio_read_block          =  hosted_sdio_read_block         ,
	._h_sdio_write_block         =  hosted_sdio_write_block        ,
	._h_sdio_wait_slave_intr     =  hosted_sdio_wait_slave_intr    ,
#endif
#ifdef CONFIG_ESP_SPI_HD_HOST_INTERFACE
	._h_bus_init                 =  hosted_spi_hd_init               ,
	._h_spi_hd_read_reg          =  hosted_spi_hd_read_reg           ,
	._h_spi_hd_write_reg         =  hosted_spi_hd_write_reg          ,
	._h_spi_hd_read_dma          =  hosted_spi_hd_read_dma           ,
	._h_spi_hd_write_dma         =  hosted_spi_hd_write_dma          ,
	._h_spi_hd_set_data_lines    =  hosted_spi_hd_set_data_lines     ,
	._h_spi_hd_send_cmd9         =  hosted_spi_hd_send_cmd9          ,
#endif
#ifdef CONFIG_ESP_UART_HOST_INTERFACE
	._h_bus_init                 = hosted_uart_init                ,
	._h_uart_read                = hosted_uart_read                ,
	._h_uart_write               = hosted_uart_write               ,
	._h_uart_wait_rx_data        = hosted_wait_rx_data             ,
#endif
};
