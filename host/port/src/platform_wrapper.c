// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdint.h>
#include <string.h>
#include "common/trace.h"
#include "serial_if.h"
#include "serial_ll_if.h"
#include "platform_wrapper.h"

#define DBG_TAG           "esp.platform"
#define DBG_LVL           DBG_INFO
#include "rtdbg.h"


#define HOSTED_CALLOC(buff,nbytes) do {                           \
    buff = (uint8_t *)hosted_calloc(1, nbytes);                   \
    if (!buff) {                                                  \
        LOG_E("%s, Failed to allocate memory ", __func__);     \
        goto free_bufs;                                           \
    }                                                             \
} while(0);


static rt_sem_t readSemaphore;
static serial_ll_handle_t * serial_ll_if_g;

static void control_path_rx_indication(void);

struct serial_drv_handle_t {
	int handle; /* dummy variable */
};

struct timer_handle_t {
	rt_timer_t timer_id;
};

int control_path_platform_init(void)
{
	/* control path semaphore */
	readSemaphore = rt_sem_create("esp_rx", 0, RT_IPC_FLAG_PRIO);
	assert(readSemaphore);

	/* grab the semaphore, so that task will be mandated to wait on semaphore */
	// if (rt_sem_take(readSemaphore , RT_WAITING_FOREVER) != RT_EOK) {
	// 	LOG_E("could not obtain readSemaphore");
	// 	return STM_FAIL;
	// }

	serial_ll_if_g = serial_ll_init(control_path_rx_indication);
	if (!serial_ll_if_g) {
		LOG_E("Serial interface creation failed");
		assert(serial_ll_if_g);
		return STM_FAIL;
	}
	if (STM_OK != serial_ll_if_g->fops->open(serial_ll_if_g)) {
		LOG_E("Serial interface open failed");
		return STM_FAIL;
	}
	return STM_OK;
}

int control_path_platform_deinit(void)
{
	if (STM_OK != serial_ll_if_g->fops->close(serial_ll_if_g)) {
		LOG_E("Serial interface close failed");
		return STM_FAIL;
	}
	return STM_OK;
}

static void control_path_rx_indication(void)
{
	/* heads up to control path for read */
	if(readSemaphore) {
		rt_sem_release(readSemaphore);
	}
}

/* -------- Memory ---------- */
void* hosted_malloc(size_t size)
{
	return rt_malloc(size);
}

void* hosted_calloc(size_t blk_no, size_t size)
{
	return rt_calloc(blk_no, size);
}

void hosted_free(void* ptr)
{
	if(ptr) {
		rt_free(ptr);
		ptr=NULL;
	}
}

void *hosted_realloc(void *mem, size_t newsize)
{
	void *p = NULL;

	if (newsize == 0) {
		mem_free(mem);
		return NULL;
	}

	p = hosted_malloc(newsize);
	if (p) {
		/* zero the memory */
		if (mem != NULL) {
			memcpy(p, mem, newsize);
			mem_free(mem);
		}
	}
	return p;
}

/* -------- Threads ---------- */
void *hosted_thread_create(void (*start_routine)(void const *), void *arg)
{
	if (!start_routine) {
		LOG_E("start_routine is mandatory for thread create");
		return NULL;
	}

	rt_thread_t thread = rt_thread_create("esp-hosted", (void(*)(void *))start_routine, arg, ESP_HOSTED_THREAD_STACK_SIZE, ESP_HOSTED_THREAD_PRIORITY, 10);

	if (!thread) {
		LOG_E("Failed to allocate thread handle");
		return NULL;
	}

	if (rt_thread_startup(thread) != RT_EOK) {
		LOG_E("Failed to create ctrl path task");
		rt_thread_delete(thread);
		return NULL;
	}

	return thread;
}

int hosted_thread_cancel(void *thread_handle)
{
	return rt_thread_suspend(thread_handle);
}

/* -------- Semaphores ---------- */
void * hosted_create_semaphore(int init_value)
{
	return rt_sem_create("esp_ctrl", init_value, RT_IPC_FLAG_PRIO);
}

unsigned int sleep(unsigned int seconds) {
   rt_thread_mdelay(seconds * 1000);
   return 0;
}

unsigned int msleep(unsigned int mseconds) {
   rt_thread_mdelay(mseconds);
   return 0;
}

int hosted_get_semaphore(void * semaphore_handle, int timeout)
{
	if (!semaphore_handle) {
		LOG_E("Uninitialized semaphore");
		return STM_FAIL;
	}

	if (timeout == HOSTED_SEM_NON_BLOCKING) {
		/* non blocking */
		return rt_sem_take(semaphore_handle, 0);
	} else if (timeout == RT_WAITING_FOREVER) {
		/* Blocking */
		return rt_sem_take(semaphore_handle, RT_WAITING_FOREVER);
	} else {
		return rt_sem_take(semaphore_handle, rt_tick_from_millisecond(timeout * 1000));
	}
}

int hosted_post_semaphore(void * semaphore_handle)
{
	if (!semaphore_handle) {
		LOG_E("Uninitialized semaphore");
		return STM_FAIL;
	}

	return rt_sem_release(semaphore_handle);
}

int hosted_destroy_semaphore(void * semaphore_handle)
{
	if (!semaphore_handle) {
		LOG_E("Uninitialized semaphore");
		return STM_FAIL;
	}

	if (rt_sem_delete(semaphore_handle) != RT_EOK)
	{
		LOG_E("Failed to destroy semaphore");
		return STM_FAIL;
	}

	return STM_OK;
}
/* -------- Timers  ---------- */
int hosted_timer_stop(void *timer_handle)
{
	if (!timer_handle) {
		LOG_E("Uninitialized timer handle");
		return STM_FAIL;
	}

	if (rt_timer_stop(timer_handle) != RT_EOK)
	{
		LOG_E("Unable to stop timer");
		return STM_FAIL;
	}

	if (rt_timer_delete(timer_handle) != RT_EOK)
	{
		LOG_E("Unable to delete timer");
		return STM_FAIL;
	}

	return STM_OK;
}

/* Sample timer_handler looks like this:
 *
 * void expired(union sigval timer_data){
 *     struct mystruct *a = timer_data.sival_ptr;
 * 	printf("Expired %u", a->mydata++);
 * }
 **/

void *hosted_timer_start(int duration, int type,
		void (*timeout_handler)(void const *), void *arg)
{
	rt_timer_t timer_handle = rt_timer_create("esp_ctrl", (void (*)(void *))timeout_handler, arg, 
							rt_tick_from_millisecond(duration * 1000),
							type);

	if (timer_handle == NULL)
	{
		LOG_E("Failed to allocate timer handle");
		return NULL;
	}

	if (rt_timer_start(timer_handle) != RT_EOK)
	{
		LOG_E("Failed to start timer, destroying timer");
		rt_timer_delete(timer_handle);
		return NULL;
	}

	return timer_handle;
}

/* -------- Serial Drv ---------- */
struct serial_drv_handle_t* serial_drv_open(const char *transport)
{
	static struct serial_drv_handle_t* serial_drv_handle = NULL;
	
	if (!transport) {
		LOG_E("Invalid parameter in open ");
		return NULL;
	}

	if (serial_drv_handle) {
		LOG_D("return orig handle");
		return serial_drv_handle;
	}

	serial_drv_handle = hosted_calloc(1,sizeof(struct serial_drv_handle_t));
	if (!serial_drv_handle) {
		LOG_E("Failed to allocate memory ");
		return NULL;
	}

	return serial_drv_handle;
}

int serial_drv_write (struct serial_drv_handle_t* serial_drv_handle,
	uint8_t* buf, int in_count, int* out_count)
{
	int ret = 0;
	if (!serial_drv_handle || !buf || !in_count || !out_count) {
		LOG_E("Invalid parameters in write");
		return STM_FAIL;
	}

	if( (!serial_ll_if_g) ||
		(!serial_ll_if_g->fops) ||
		(!serial_ll_if_g->fops->write)) {
		LOG_E("serial interface not valid");
		return STM_FAIL;
	}

	ret = serial_ll_if_g->fops->write(serial_ll_if_g, buf, in_count);
	if (ret != STM_OK) {
		*out_count = 0;
		LOG_E("Failed to write data");
		return STM_FAIL;
	}

	*out_count = in_count;
	return STM_OK;
}


uint8_t * serial_drv_read(struct serial_drv_handle_t *serial_drv_handle,
		uint32_t *out_nbyte)
{
	uint16_t init_read_len = 0;
	uint16_t rx_buf_len = 0;
	uint8_t* read_buf = NULL;
	int ret = 0;
	/* Any of `CTRL_EP_NAME_EVENT` and `CTRL_EP_NAME_RESP` could be used,
	 * as both have same strlen in adapter.h */
	const char* ep_name = CTRL_EP_NAME_RESP;
	uint8_t *buf = NULL;
	uint32_t buf_len = 0;


	if (!serial_drv_handle || !out_nbyte) {
		LOG_E("Invalid parameters in read");
		return NULL;
	}

	*out_nbyte = 0;

	if(!readSemaphore) {
		LOG_E("Semaphore not initialized");
		return NULL;
	}
	if (rt_sem_take(readSemaphore, HOSTED_SEM_BLOCKING) != RT_EOK) {
		LOG_E("Failed to read data ");
		return NULL;
	}

	if( (!serial_ll_if_g) ||
		(!serial_ll_if_g->fops) ||
		(!serial_ll_if_g->fops->read)) {
		LOG_E("serial interface refusing to read");
		return NULL;
	}

	/* Get buffer from serial interface */
	read_buf = serial_ll_if_g->fops->read(serial_ll_if_g, &rx_buf_len);
	if ((!read_buf) || (!rx_buf_len)) {
		LOG_E("serial read failed");
		return NULL;
	}
	print_hex_dump(read_buf, rx_buf_len, "Serial read data");

/*
 * Read Operation happens in two steps because total read length is unknown
 * at first read.
 *      1) Read fixed length of RX data
 *      2) Read variable length of RX data
 *
 * (1) Read fixed length of RX data :
 * Read fixed length of received data in below format:
 * ----------------------------------------------------------------------------
 *  Endpoint Type | Endpoint Length | Endpoint Value  | Data Type | Data Length
 * ----------------------------------------------------------------------------
 *
 *  Bytes used per field as follows:
 *  ---------------------------------------------------------------------------
 *      1         |       2         | Endpoint Length |     1     |     2     |
 *  ---------------------------------------------------------------------------
 *
 *  int_read_len = 1 + 2 + Endpoint length + 1 + 2
 */

	init_read_len = SIZE_OF_TYPE + SIZE_OF_LENGTH + strlen(ep_name) +
		SIZE_OF_TYPE + SIZE_OF_LENGTH;

	if(rx_buf_len < init_read_len) {
		mem_free(read_buf);
		LOG_E("Incomplete serial buff, return");
		return NULL;
	}

	HOSTED_CALLOC(buf,init_read_len);

	memcpy(buf, read_buf, init_read_len);

	/* parse_tlv function returns variable payload length
	 * of received data in buf_len
	 **/
	ret = parse_tlv(buf, &buf_len);
	if (ret || !buf_len) {
		mem_free(buf);
		LOG_E("Failed to parse RX data ");
		goto free_bufs;
	}

	if (rx_buf_len < (init_read_len + buf_len)) {
		LOG_E("Buf read on serial iface is smaller than expected len");
		mem_free(buf);
		goto free_bufs;
	}

	mem_free(buf);
/*
 * (2) Read variable length of RX data:
 */
	HOSTED_CALLOC(buf,buf_len);

	memcpy((buf), read_buf+init_read_len, buf_len);

	mem_free(read_buf);

	*out_nbyte = buf_len;
	return buf;

free_bufs:
	mem_free(read_buf);
	mem_free(buf);
	return NULL;
}

int serial_drv_close(struct serial_drv_handle_t** serial_drv_handle)
{
	if (!serial_drv_handle || !(*serial_drv_handle)) {
		LOG_E("Invalid parameter in close");
		if (serial_drv_handle)
			mem_free(serial_drv_handle);
		return STM_FAIL;
	}
	mem_free(*serial_drv_handle);
	return STM_OK;
}
