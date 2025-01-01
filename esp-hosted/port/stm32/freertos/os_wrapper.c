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

#include <stdlib.h>
#include "string.h"
#include "esp_log.h"
#include "os_wrapper.h"
#include "esp_hosted_config.h"
#include "transport_drv.h"
#include "esp_event.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "semphr.h"
#include "stats.h"
#include <sys/errno.h>
#include "esp_hosted_api.h"
#include "esp_log_level.h"

DEFINE_LOG_TAG(os_wrapper);

extern void * spi_handle;

ESP_EVENT_DEFINE_BASE(WIFI_EVENT);
struct hosted_config_t g_h = HOSTED_CONFIG_INIT_DEFAULT();

struct mempool * nw_mp_g = NULL;

struct timer_handle_t {
	TimerHandle_t timer_id;
};

#define HOSTED_MEMORY_ANALYZER 0

#if HOSTED_MEMORY_ANALYZER
struct mem_analyser {
	char func[10];
	uint16_t line;
};

struct mem_analyser mem_points[500] = {0};
#endif
/* -------- Memory ---------- */

void * hosted_memcpy(void* dest, const void* src, uint32_t size)
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

void * hosted_memset(void* buf, int val, size_t len)
{
	return memset(buf, val, len);
}

void* hosted_malloc(size_t size)
{
	void *mem = NULL;
	vPortEnterCritical();
	mem = MALLOC(size);
	vPortExitCritical();
	assert(mem);

#if H_MEM_STATS
			h_stats_g.total.malloc++;
#endif
	return mem;
}

void* hosted_calloc(size_t blk_no, size_t size)
{
	void* ptr = NULL;
	ptr = hosted_malloc(blk_no*size);
	if (!ptr) {
		return NULL;
	}
#if H_MEM_STATS
			h_stats_g.total.calloc++;
#endif

	hosted_memset(ptr, 0, blk_no*size);
	return ptr;
}

void hosted_free(void* ptr)
{
	vPortEnterCritical();
	FREE(ptr);
	vPortExitCritical();
#if H_MEM_STATS
			h_stats_g.total.freed++;
#endif
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

#if H_MEM_STATS
			h_stats_g.total.realloc++;
#endif

	return p;
}

/* -------- Threads ---------- */

void *hosted_thread_create(char *tname, uint32_t tprio, uint32_t tstack_size, void (*start_routine)(void const *), void *sr_arg)
{
	int task_created = RET_OK;

	if (!start_routine) {
		ESP_LOGE(TAG, "start_routine is mandatory for thread create\n");
		return NULL;
	}

	thread_handle_t *thread_handle = (thread_handle_t *)hosted_malloc(
			sizeof(thread_handle_t));
	if (!thread_handle) {
		ESP_LOGE(TAG, "Failed to allocate thread handle\n");
		return NULL;
	}

#if 0
	osThreadDef(
			Ctrl_port_tsk,
			start_routine,
			CTRL_PATH_TASK_PRIO, 0,
			CTRL_PATH_TASK_STACK_SIZE);
	*thread_handle = osThreadCreate(osThread(Ctrl_port_tsk), arg);
#endif
	task_created = xTaskCreate((void (*)(void *))start_routine, tname, tstack_size, sr_arg, tprio, &thread_handle->t_hl);
	if (!(thread_handle->t_hl)) {
		ESP_LOGE(TAG, "Failed to create thread: %s\n", tname);
		HOSTED_FREE(thread_handle);
		return NULL;
	}

	if (task_created != pdTRUE) {
		ESP_LOGE(TAG, "Failed 2 to create thread: %s\n", tname);
		HOSTED_FREE(thread_handle);
		return NULL;
	}

	return thread_handle;
}

int hosted_thread_cancel(void *arg)
{
	thread_handle_t *thread_handle = (thread_handle_t*)arg;
	if (!thread_handle || !thread_handle->t_hl) {
		ESP_LOGE(TAG, "Invalid thread handle\n");
		return RET_INVALID;
	}


	//ret = osThreadTerminate(*thread_hdl);
	//if (ret) {
	//	ESP_LOGE(TAG, "Prob in pthread_cancel, destroy handle anyway\n");
	//	hosted_free(thread_handle);
	//	return RET_INVALID;
	//}
	vTaskDelete(thread_handle->t_hl);

	HOSTED_FREE(thread_handle);
	return RET_OK;
}

/* -------- Sleeps -------------- */
unsigned int hosted_msleep(unsigned int mseconds)
{
   //osDelay(mseconds);
   vTaskDelay(pdMS_TO_TICKS(mseconds));
   //usleep(mseconds*1000UL);
   return 0;
}

unsigned int hosted_sleep(unsigned int seconds)
{
   //osDelay(seconds * 1000);
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
/* User expected to pass item's address to this func eg. &item */
int hosted_queue_item(void * arg, void *item, int timeout)
{
	int item_added_in_back = 0;

	queue_handle_t *queue_handle = (queue_handle_t *)arg;
	if (!queue_handle) {
		ESP_LOGE(TAG, "Uninitialized sem id 3\n");
		return RET_INVALID;
	}

	//return osSemaphoreRelease(*q_id);
	item_added_in_back = xQueueSendToBack(queue_handle->q_hl, item, timeout);
	if (pdTRUE == item_added_in_back)
		return RET_OK;

	return RET_FAIL;
}

void * hosted_create_queue(uint32_t qnum_elem, uint32_t qitem_size)
{
	queue_handle_t *q_id = NULL;
	//osSemaphoreDef(sem_template_ctrl);

	q_id = (queue_handle_t*)hosted_malloc(
			sizeof(queue_handle_t));
	if (!q_id) {
		ESP_LOGE(TAG, "Q allocation failed\n");
		return NULL;
	}

	//*q_id = osSemaphoreCreate(osSemaphore(sem_template_ctrl) , 1);

	q_id->q_hl = xQueueCreate(qnum_elem, qitem_size);
	if (!q_id->q_hl) {
		ESP_LOGE(TAG, "Q create failed\n");
		return NULL;
	}

	return q_id;
}


/* User expected to pass item's address to this func eg. &item */
int hosted_dequeue_item(void * arg, void *item, int timeout)
{
	int item_retrieved = 0;

	queue_handle_t *queue_handle = (queue_handle_t *)arg;
	if (!queue_handle) {
		ESP_LOGE(TAG, "Uninitialized Q id 1\n\r");
		return RET_INVALID;
	}

	if (!queue_handle->q_hl) {
		ESP_LOGE(TAG, "Uninitialized Q id 2\n\r");
		return RET_INVALID;
	}

	if (!timeout) {
		/* non blocking */
		//return osSemaphoreWait(*q_id, 0);
		item_retrieved = xQueueReceive(queue_handle->q_hl, item, 0);
	} else if (timeout<0) {
		/* Blocking */
		//return osSemaphoreWait(*q_id, osWaitForever);
		item_retrieved = xQueueReceive(queue_handle->q_hl, item, HOSTED_BLOCK_MAX);
	} else {
		//return osSemaphoreWait(*q_id, SEC_TO_MILLISEC(timeout));
		////TODO: uncomment below line
		item_retrieved = xQueueReceive(queue_handle->q_hl, item, pdMS_TO_TICKS(SEC_TO_MILLISEC(timeout)));
		//item_retrieved = xQueueReceive(*q_id, HOSTED_BLOCK_MAX);
	}

	if (item_retrieved == pdTRUE)
		return 0;

	return RET_FAIL;
}

int hosted_destroy_queue(void * arg)
{
	int ret = RET_OK;
	queue_handle_t *queue_handle = (queue_handle_t *)arg;

	if (!queue_handle || !queue_handle->q_hl) {
		ESP_LOGE(TAG, "Uninitialized Q id 4\n");
		return RET_INVALID;
	}


	//ret = osSemaphoreDelete(*q_id);
	//ret = osSemaphoreDelete(*q_id);
	vQueueDelete(queue_handle->q_hl);
	//if(ret)
	//	ESP_LOGE(TAG, "Failed to destroy Q\n");

	HOSTED_FREE(queue_handle);

	return ret;
}


int hosted_reset_queue(void * arg)
{
	queue_handle_t *queue_handle = (queue_handle_t *)arg;

	if (!queue_handle || !queue_handle->q_hl) {
		ESP_LOGE(TAG, "Uninitialized Q id 5\n");
		return RET_INVALID;
	}


	//ret = osSemaphoreDelete(*q_id);
	//ret = osSemaphoreDelete(*q_id);
	return xQueueReset(queue_handle->q_hl);
}

/* -------- Mutex --------------- */

int hosted_unlock_mutex(void * arg)
{
	int mut_unlocked = 0;
	mutex_handle_t * mutex_handle = (mutex_handle_t*) arg;

	if (!mutex_handle) {
		ESP_LOGE(TAG, "Uninitialized mut id 3\n");
		return RET_INVALID;
	}

	//return osSemaphoreRelease(*mut_id);

	mut_unlocked = xSemaphoreGive(mutex_handle->m_hl);
	if (mut_unlocked)
		return 0;

	return RET_FAIL;
}

void * hosted_create_mutex(void)
{
	mutex_handle_t *mut_id = NULL;
	//osSemaphoreDef(sem_template_ctrl);

	mut_id = (mutex_handle_t*)hosted_malloc(
			sizeof(mutex_handle_t));

	if (!mut_id) {
		ESP_LOGE(TAG, "mut allocation failed\n");
		return NULL;
	}

	//*mut_id = osSemaphoreCreate(osSemaphore(sem_template_ctrl) , 1);
	mut_id->m_hl = xSemaphoreCreateMutex();
	if (!mut_id->m_hl) {
		ESP_LOGE(TAG, "mut create failed\n");
		return NULL;
	}

	//hosted_unlock_mutex(*mut_id);

	return mut_id;
}


int hosted_lock_mutex(void * arg, int timeout)
{
	mutex_handle_t * mutex_handle = (mutex_handle_t*) arg;
	int mut_locked = 0;

	if (!mutex_handle) {
		ESP_LOGE(TAG, "Uninitialized mut id 1\n\r");
		return RET_INVALID;
	}

	if (!mutex_handle->m_hl) {
		ESP_LOGE(TAG, "Uninitialized mut id 2\n\r");
		return RET_INVALID;
	}

	//return osSemaphoreWait(*mut_id, osWaitForever); //??
	mut_locked = xSemaphoreTake(mutex_handle->m_hl, HOSTED_BLOCK_MAX);
	if (mut_locked == pdTRUE)
		return 0;

	return RET_FAIL;
}

int hosted_destroy_mutex(void * arg)
{
	mutex_handle_t * mutex_handle = (mutex_handle_t*) arg;

	if (!mutex_handle || !mutex_handle->m_hl) {
		ESP_LOGE(TAG, "Uninitialized mut id 4\n");
		return RET_INVALID;
	}

	//ret = osSemaphoreDelete(*mut_id);
	//ret = osSemaphoreDelete(*mut_id);
	vSemaphoreDelete(mutex_handle->m_hl);
	//if(ret)
	//	ESP_LOGE(TAG, "Failed to destroy sem\n");

	HOSTED_FREE(mutex_handle);

	return RET_OK;
}

/* -------- Semaphores ---------- */
int hosted_post_semaphore(void * arg)
{
	int sem_posted = 0;
	semaphore_handle_t * semaphore_handle = (semaphore_handle_t*) arg;

	if (!semaphore_handle) {
		ESP_LOGE(TAG, "Uninitialized sem id 3\n");
		return RET_INVALID;
	}

	//return osSemaphoreRelease(*sem_id);
	sem_posted = xSemaphoreGive(semaphore_handle->s_hl);
	if (pdTRUE == sem_posted)
		return RET_OK;

	return RET_FAIL;
}

int hosted_post_semaphore_from_isr(void * arg)
{
	semaphore_handle_t * semaphore_handle = (semaphore_handle_t*) arg;

	int sem_posted = 0;
	BaseType_t mustYield = pdFALSE;

	if (!semaphore_handle) {
		ESP_LOGE(TAG, "Uninitialized sem id 3\n");
		return RET_INVALID;
	}

	//return osSemaphoreRelease(*sem_id);
    sem_posted = xSemaphoreGiveFromISR(semaphore_handle->s_hl, &mustYield);
    if (mustYield) {
        portYIELD_FROM_ISR(mustYield);
    }
	if (pdTRUE == sem_posted)
		return RET_OK;

	return RET_FAIL;
}

void * hosted_create_semaphore(int maxCount)
{
	semaphore_handle_t *semaphore_handle = NULL;
	//osSemaphoreDef(sem_template_ctrl);

	semaphore_handle = (semaphore_handle_t*)hosted_malloc(
			sizeof(semaphore_handle_t));
	if (!semaphore_handle) {
		ESP_LOGE(TAG, "Sem allocation failed\n");
		return NULL;
	}

	//*sem_id = osSemaphoreCreate(osSemaphore(sem_template_ctrl) , 1);

	if (maxCount>1)
		semaphore_handle->s_hl = xSemaphoreCreateCounting(maxCount,0);
	else
		semaphore_handle->s_hl = xSemaphoreCreateBinary();

	if (!semaphore_handle->s_hl) {
		ESP_LOGE(TAG, "sem create failed\n");
		return NULL;
	}

	xSemaphoreGive(semaphore_handle->s_hl);

	return semaphore_handle;
}


int hosted_get_semaphore(void * arg, int timeout)
{
	semaphore_handle_t * semaphore_handle = (semaphore_handle_t*) arg;

	int sem_acquired = 0;

	if (!semaphore_handle) {
		ESP_LOGE(TAG, "Uninitialized sem id 1\n\r");
		return RET_INVALID;
	}

	if (!semaphore_handle->s_hl) {
		ESP_LOGE(TAG, "Uninitialized sem id 2\n\r");
		return RET_INVALID;
	}

	if (!timeout) {
		/* non blocking */
		//return osSemaphoreWait(*sem_id, 0);
		sem_acquired = xSemaphoreTake(semaphore_handle->s_hl, 0);
	} else if (timeout<0) {
		/* Blocking */
		//return osSemaphoreWait(*sem_id, osWaitForever);
		sem_acquired = xSemaphoreTake(semaphore_handle->s_hl, HOSTED_BLOCK_MAX);
	} else {
		//return osSemaphoreWait(*sem_id, SEC_TO_MILLISEC(timeout));
		////TODO: uncomment below line
		//sem_acquired = xSemaphoreTake(*sem_id, pdMS_TO_TICKS(SEC_TO_MILLISEC(timeout)));
		sem_acquired = xSemaphoreTake(semaphore_handle->s_hl, pdMS_TO_TICKS(SEC_TO_MILLISEC(timeout)));
	}

	if (sem_acquired == pdTRUE)
		return 0;

	return RET_FAIL;
}

int hosted_destroy_semaphore(void * arg)
{
	int ret = RET_OK;
	semaphore_handle_t * semaphore_handle = (semaphore_handle_t*) arg;

	if (!semaphore_handle || !semaphore_handle->s_hl) {
		ESP_LOGE(TAG, "Uninitialized sem id 4\n");
		return RET_INVALID;
	}

	//ret = osSemaphoreDelete(*sem_id);
	//ret = osSemaphoreDelete(*sem_id);
	vSemaphoreDelete(semaphore_handle->s_hl);
	//if(ret)
	//	ESP_LOGE(TAG, "Failed to destroy sem\n");

	HOSTED_FREE(semaphore_handle);

	return ret;
}


/* -------- Timers  ---------- */

struct timer_argument_t {
	void *arg;
	int type;
	void (*timeout_handler)(void *);
};

int hosted_timer_stop(void *tmr_hdl)
{
	struct timer_handle_t *timer_handle = (struct timer_handle_t*)tmr_hdl;

	ESP_LOGV(TAG, "Stop the timer\n");
	if (!timer_handle || !timer_handle->timer_id)
		return RET_FAIL;

	if (xTimerIsTimerActive(timer_handle->timer_id) != pdFALSE) {
		xTimerStop(timer_handle->timer_id, 0);
	}

    struct timer_argument_t * private_arg  =
		(struct timer_argument_t *)pvTimerGetTimerID(timer_handle->timer_id);

    HOSTED_FREE(private_arg);
	xTimerDelete(timer_handle->timer_id, 0);
	HOSTED_FREE(timer_handle->timer_id);
	HOSTED_FREE(timer_handle);

	return ESP_OK;
}

/* Sample timer_handler looks like this:
 *
 * void expired(union sigval timer_data){
 *     struct mystruct *a = timer_data.sival_ptr;
 * 	ESP_LOGE(TAG, "Expired %u\n", a->mydata++);
 * }
 **/

static void vTimerCallback( TimerHandle_t xTimer )
 {
    /* Optionally do something if the pxTimer parameter is NULL. */
    assert(xTimer);

    /* The number of times this timer has expired is saved as the
    timer's ID.  Obtain the count. */
    struct timer_argument_t * private_arg  = (struct timer_argument_t *)pvTimerGetTimerID(xTimer);
	assert(private_arg);

	if (private_arg->timeout_handler)
		private_arg->timeout_handler(private_arg->arg);

	if (!private_arg->type)
		HOSTED_FREE( private_arg);

 }


//void *hosted_timer_start(int duration, int type,
//		void (*timeout_handler)(void const *), void *arg)
void *hosted_timer_start(int duration_sec, int type,
		void (*timeout_handler)(void *), void *arg)
{
	struct timer_handle_t *timer_handle = NULL;

	ESP_LOGV(TAG, "Start the timer %u sec\n", duration_sec);

	/* create argument */
	struct timer_argument_t *private_arg = (struct timer_argument_t *)hosted_calloc(
			1, sizeof(struct timer_argument_t));
	if (!private_arg)
		goto free_timer;

	private_arg->arg = arg;
	private_arg->type = (type == RPC__TIMER_PERIODIC);
	private_arg->timeout_handler = timeout_handler;


	/* alloc */
	timer_handle = (struct timer_handle_t *)hosted_calloc(
			1, sizeof(struct timer_handle_t));
	if (!timer_handle)
		goto free_timer;

	/* create */
	timer_handle->timer_id = xTimerCreate(
			"hosted_timer",
			pdMS_TO_TICKS( SEC_TO_MILLISEC(duration_sec) ),
			(type == RPC__TIMER_PERIODIC),
			private_arg,
			vTimerCallback);

	if (!timer_handle->timer_id)
		goto free_timer;

	if (xTimerStart(timer_handle->timer_id, 0) != pdPASS)
		goto free_timer;

	return timer_handle;

free_timer:
	ESP_LOGE(TAG, "Failed to create timer\n");
	HOSTED_FREE(private_arg);
	if (timer_handle->timer_id)
		xTimerDelete(timer_handle->timer_id, 0);
	HOSTED_FREE(timer_handle->timer_id);
	HOSTED_FREE(timer_handle);
	return NULL;
}

void* hosted_create_lock_mempool(void)
{
	return g_h.funcs->_h_create_mutex();
}
void hosted_lock_mempool(void *lock_handle)
{
	g_h.funcs->_h_lock_mutex(lock_handle, HOSTED_BLOCK_MAX);
}

void hosted_unlock_mempool(void *lock_handle)
{
	g_h.funcs->_h_unlock_mutex(lock_handle);
}

/* GPIO */

int hosted_config_gpio(void* gpio_port_in, uint32_t gpio_num, uint32_t mode)
{
	gpio_port_handle_t gpio_port = (gpio_port_handle_t) gpio_port_in;
	GPIO_InitTypeDef GPIO_InitStruct;

    /* GPIO Ports Clock Enable */
	if (GPIOA == gpio_port)
		__HAL_RCC_GPIOA_CLK_ENABLE();
	else if (GPIOB == gpio_port)
		__HAL_RCC_GPIOB_CLK_ENABLE();

    /* TODO: make this pin configurable from project config */
    GPIO_InitStruct.Pin = gpio_num;
    GPIO_InitStruct.Mode = mode;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(gpio_port, &GPIO_InitStruct);
	return 0;
}

struct gpio_interrupts {
	uint32_t gpio_num;
	void (*gpio_isr_handler)(void* arg);
};

#define MAX_GPIO_ISR_REGISTERED 2
struct gpio_interrupts gpio_isr_arr[MAX_GPIO_ISR_REGISTERED];
uint8_t num_gpio_isr_registered = 0;

int hosted_config_gpio_as_interrupt(void* gpio_port_in, uint32_t gpio_num, uint32_t intr_type, void (*gpio_isr_handler)(void* arg))
{
	/* GPIOs Data Ready and Handshake are already configured as ISR through ioc*/
	assert(num_gpio_isr_registered < MAX_GPIO_ISR_REGISTERED);
	gpio_isr_arr[num_gpio_isr_registered].gpio_num = gpio_num;
	gpio_isr_arr[num_gpio_isr_registered++].gpio_isr_handler = gpio_isr_handler;
	return 0;
}

int hosted_read_gpio(void* gpio_port_in, uint32_t gpio_num)
{
	gpio_port_handle_t gpio_port = (gpio_port_handle_t)gpio_port_in;
    return HAL_GPIO_ReadPin(gpio_port, gpio_num);
}

int hosted_write_gpio(void* gpio_port_in, uint32_t gpio_num, uint32_t value)
{
	gpio_port_handle_t gpio_port = (gpio_port_handle_t)gpio_port_in;
    HAL_GPIO_WritePin(gpio_port, gpio_num, value);
    return  0;
}




void hosted_init_hook(void)
{
	/* This is hook to initialize port specific contexts, if any */
	ESP_LOGI(TAG, "\n\n\r");
	ESP_LOGI(TAG, "********************************************************");
	ESP_LOGI(TAG, "********             ESP-Hosted                 ********");
	ESP_LOGI(TAG, "********************************************************");
	ESP_LOGI(TAG, "Transport - SPI\n\rSPI master, CLK Freq:<Check IOC File, probably 10MHz>");
	ESP_LOGI(TAG, "Mode: CLK Phase:%u Polarity:%u\n",
			 SPI_BUS_HAL.Init.CLKPhase?1:0,SPI_BUS_HAL.Init.CLKPolarity?1:0);
#if 0
	ESP_LOGI(TAG, "GPIOs: MOSI:%s-%u MISO:%s-%u CLK:%s-%u CS:%s-%u Handshake:%s-%u Data_Ready:%s-%u ResetPin:%s-%u",
			STR_FROM_VAL(GPIO_MOSI_GPIO_Port), GPIO_MOSI_Pin,
			STR_FROM_VAL(GPIO_MISO_GPIO_Port), GPIO_MISO_Pin,
			STR_FROM_VAL(GPIO_CLK_GPIO_Port), GPIO_CLK_Pin,
			STR_FROM_VAL(GPIO_CS_GPIO_Port), GPIO_CS_Pin,
			STR_FROM_VAL(GPIO_HANDSHAKE_GPIO_Port), GPIO_HANDSHAKE_Pin,
			STR_FROM_VAL(GPIO_DATA_READY_GPIO_Port), GPIO_DATA_READY_Pin,
			STR_FROM_VAL(GPIO_RESET_GPIO_Port), GPIO_RESET_Pin
			);
#endif




}

#define UART_TRANSMIT(ch) HAL_UART_Transmit(&huart3, (uint8_t *)&ch, 1, 0xFFFF);
#define UART_RECEIVE(ch) HAL_UART_Receive(&huart3, (uint8_t *)&ch, 1, 0xFFFF);

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	uint8_t i = 0;
	for (i = 0; i< num_gpio_isr_registered; i++) {
		if (GPIO_Pin == gpio_isr_arr[i].gpio_num)
			if (gpio_isr_arr[i].gpio_isr_handler) {
				ESP_LOGV(TAG, "Intr %ld GPIO\n\r", gpio_isr_arr[i].gpio_num);
				gpio_isr_arr[i].gpio_isr_handler(NULL);
			}
	}
}

#if STM32_SPI_INTR
// semaphore for SPI transaction complete
void * spi_trans_complete_sem = NULL;
// SPI Callback Handler
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	hosted_post_semaphore_from_isr(spi_trans_complete_sem);
}
#endif

// mutex to lock SPI transaction
void * spi_trans_mutex = NULL;

void * hosted_spi_init(void)
{
	HAL_GPIO_WritePin(USR_SPI_CS_GPIO_Port, USR_SPI_CS_Pin, GPIO_PIN_SET);

#if STM32_SPI_INTR
	spi_trans_complete_sem = hosted_create_semaphore(1);
	if (NULL == spi_trans_complete_sem) {
		ESP_LOGE(TAG, "failed to create SPI transaction semaphore");
		return NULL;
	}
	hosted_get_semaphore(spi_trans_complete_sem, HAL_MAX_DELAY);
#endif

	spi_trans_mutex = hosted_create_mutex();
	if (NULL == spi_trans_mutex) {
		ESP_LOGE(TAG, "failed to create SPI transaction mutex");
		return NULL;
	}

	return &SPI_BUS_HAL;

#if 0
#define SPI_MODE                          SPI_MODE2
#define SPI_INIT_CLK_MHZ                  5

#ifdef CONFIG_IDF_TARGET_ESP32
#define SENDER_HOST                                  HSPI_HOST

#else
#define SENDER_HOST                                  SPI2_HOST

#endif

    esp_err_t ret;

	osSemaphore = xSemaphoreCreateBinary();
	assert(osSemaphore);

    HOSTED_CREATE_HANDLE(spi_device_handle_t, spi_handle);
    assert(spi_handle);


    //Configuration for the SPI bus
    spi_bus_config_t buscfg={
        .mosi_io_num=GPIO_MOSI,
        .miso_io_num=GPIO_MISO,
        .sclk_io_num=GPIO_SCLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1
    };

    //Configuration for the SPI device on the other side of the bus
    spi_device_interface_config_t devcfg={
        .command_bits=0,
        .address_bits=0,
        .dummy_bits=0,
        .clock_speed_hz=MHZ_TO_HZ(SPI_INIT_CLK_MHZ),
        .duty_cycle_pos=128,        //50% duty cycle
        .mode=SPI_MODE,
        .spics_io_num=GPIO_CS,
        .cs_ena_posttrans=3,        //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
        .queue_size=3
    };
#if 0
    //GPIO config for the handshake line.
    gpio_config_t handshake_io_conf={
        .intr_type=GPIO_INTR_POSEDGE,
        .mode=GPIO_MODE_INPUT,
        .pull_up_en=1,
        .pin_bit_mask=(1<<GPIO_HANDSHAKE)
    };

    gpio_config_t dataready_io_conf={
        .intr_type=GPIO_INTR_POSEDGE,
        .mode=GPIO_MODE_INPUT,
        .pull_up_en=1,
        .pin_bit_mask=(1<<GPIO_DATA_READY)
    };

    //Create the semaphore.

    //Set up handshake line interrupt.
    gpio_config(&handshake_io_conf);
    gpio_config(&dataready_io_conf);
    gpio_install_isr_service(0);
    gpio_set_intr_type(GPIO_HANDSHAKE, GPIO_INTR_POSEDGE);
    gpio_set_intr_type(GPIO_DATA_READY, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(GPIO_HANDSHAKE, gpio_hs_dr_isr_handler, NULL);
    gpio_isr_handler_add(GPIO_DATA_READY, gpio_hs_dr_isr_handler, NULL);
#endif


    //Initialize the SPI bus and add the device we want to send stuff to.
    ret=spi_bus_initialize(SENDER_HOST, &buscfg, SPI_DMA_CH_AUTO);
    assert(ret==ESP_OK);
    ret=spi_bus_add_device(SENDER_HOST, &devcfg, spi_handle);
    assert(ret==ESP_OK);

    //Assume the slave is ready for the first transmission: if the slave started up before us, we will not detect
    //positive edge on the handshake line.
    return spi_handle;
#endif
}

#if defined(STM32_MANUAL_CS) && !STM32_MANUAL_CS
/* Hardware NSS used */
static inline stm_ret_t spi_transaction(uint8_t * txbuff, uint8_t * rxbuff, uint16_t size)
{
	HAL_StatusTypeDef retval = HAL_ERROR;

	hosted_lock_mutex(spi_trans_mutex, HAL_MAX_DELAY);

	/* SPI transaction */
	retval = HAL_SPI_TransmitReceive(&SPI_BUS_HAL, txbuff, rxbuff, size, HAL_MAX_DELAY);
	// while( SPI_BUS_HAL.State == HAL_SPI_STATE_BUSY );
	while( SPI_BUS_HAL.State != HAL_SPI_STATE_READY );

	hosted_unlock_mutex(spi_trans_mutex);

	return retval;
}
#elif defined(STM32_MANUAL_CS) && STM32_MANUAL_CS
/* Software controlled NSS */
static inline stm_ret_t spi_transaction(uint8_t * txbuff, uint8_t * rxbuff, uint16_t size)
{
	HAL_StatusTypeDef retval = HAL_ERROR;

	ESP_LOGV(TAG, "Execute SPI transaction\n");
	hosted_lock_mutex(spi_trans_mutex, HAL_MAX_DELAY);

	/* SPI transaction */
	//HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 1);
	HAL_GPIO_WritePin(USR_SPI_CS_GPIO_Port, USR_SPI_CS_Pin, GPIO_PIN_RESET);
	retval = HAL_SPI_TransmitReceive(&SPI_BUS_HAL, txbuff, rxbuff, size, HAL_MAX_DELAY);
	// while( SPI_BUS_HAL.State == HAL_SPI_STATE_BUSY );
	while( SPI_BUS_HAL.State != HAL_SPI_STATE_READY );
	HAL_GPIO_WritePin(USR_SPI_CS_GPIO_Port, USR_SPI_CS_Pin, GPIO_PIN_SET);
	//HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 0);

	hosted_unlock_mutex(spi_trans_mutex);

	return retval;
}
#elif defined(STM32_SPI_INTR) && STM32_SPI_INTR
/* Interrupt driven SPI */
#define BUFFER_ALIGNED_SIZE (((1600 + 31) / 32) * 32)
#if defined   (__GNUC__)        /* GNU Compiler */
#define ALIGN_32BYTES(buf)  buf __attribute__ ((aligned (32)))
#elif defined (__ICCARM__)    /* IAR Compiler */
#define ALIGN_32BYTES(buf) _Pragma("data_alignment=32") buf
#elif defined   (__CC_ARM)      /* ARM Compiler */
#define ALIGN_32BYTES(buf) __align(32) buf
#endif

/* for DMA transfer, need to invalidate and flush data cache
 * see https://community.st.com/t5/stm32-mcus-embedded-software/solved-stm32h7-spi-does-not-seem-to-work-with-dma/m-p/405993/highlight/true#M30425
 *
 * TODO: heap allocate data cannot use DMA successfully.
 * For now, have to copy data to pre-allocated buffers
 */
ALIGN_32BYTES(uint8_t spi_tx_buffer[BUFFER_ALIGNED_SIZE]);
ALIGN_32BYTES(uint8_t spi_rx_buffer[BUFFER_ALIGNED_SIZE]);

static inline stm_ret_t spi_transaction(uint8_t * txbuff, uint8_t * rxbuff, uint16_t size)
{
	HAL_StatusTypeDef retval = HAL_ERROR;

	hosted_lock_mutex(spi_trans_mutex, HAL_MAX_DELAY);

	HAL_GPIO_WritePin(USR_SPI_CS_GPIO_Port, USR_SPI_CS_Pin, GPIO_PIN_RESET);

	// retval = HAL_SPI_TransmitReceive_IT(&SPI_BUS_HAL, txbuff, rxbuff, size);

#if 0
	SCB_CleanDCache_by_Addr(txbuff, size);
	retval = HAL_SPI_TransmitReceive_DMA(&SPI_BUS_HAL, txbuff, rxbuff, size);
#else
	memcpy(spi_tx_buffer, txbuff, size);
	SCB_CleanDCache_by_Addr(spi_tx_buffer, BUFFER_ALIGNED_SIZE);

	retval = HAL_SPI_TransmitReceive_DMA(&SPI_BUS_HAL, spi_tx_buffer, spi_rx_buffer, size);
#endif

	// wait for callback for completion
	hosted_get_semaphore(spi_trans_complete_sem, HAL_MAX_DELAY);

#if 0
	SCB_CleanInvalidateDCache_by_Addr(rxbuff, size);
#else
	SCB_CleanInvalidateDCache_by_Addr(spi_rx_buffer, BUFFER_ALIGNED_SIZE);
	memcpy(rxbuff, spi_rx_buffer, size);
#endif

	HAL_GPIO_WritePin(USR_SPI_CS_GPIO_Port, USR_SPI_CS_Pin, GPIO_PIN_SET);

	hosted_unlock_mutex(spi_trans_mutex);

	return retval;
}
#else
#error "one of STM32_xxx in esp_hosted_config.c must be defined and set"
#endif

int hosted_do_spi_transfer(void *trans)
{
	struct hosted_transport_context_t * spi_trans = trans;

	assert(trans);

	return spi_transaction(spi_trans->tx_buf, spi_trans->rx_buf,
			spi_trans->tx_buf_size);
}

int hosted_wifi_event_post(int32_t event_id,
        void* event_data, size_t event_data_size, uint32_t ticks_to_wait)
{
	ESP_LOGD(TAG, "event %ld recvd --> event_data:%p event_data_size: %u",event_id, event_data, event_data_size);
	return esp_event_post(WIFI_EVENT, event_id, event_data, event_data_size, ticks_to_wait);
}

extern void app_main(void);
void app_task(void const *arg)
{
	H_ERROR_CHECK(esp_hosted_init());

	app_main();

//	ESP_LOGI(TAG, "Exiting the task app_main");
	//g_h.funcs->_h_thread_cancel((void *)arg);

	while(1) {
		g_h.funcs->_h_sleep(100);
	}
}
/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void)
{
	void *app_task_hdl = NULL;
	//hosted_thread_create
	app_task_hdl = hosted_thread_create("app_task", DFLT_TASK_PRIO, DFLT_TASK_STACK_SIZE, app_task, app_task_hdl);
	assert(app_task_hdl);
}

/**
  * @brief  Retargets the C library printf function to the USART.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
	/* Place your implementation of fputc here */
	/* e.g. write a character to the USART6 and
	 * Loop until the end of transmission */
	UART_TRANSMIT(ch);

	return ch;
}

uint32_t esp_log_timestamp(void)
{
    TickType_t tick_count = xTaskGetTickCount();
    return tick_count * (1000 / configTICK_RATE_HZ);
}

uint32_t esp_get_free_heap_size( void )
{
	/* stubbed function as if now */
    return xPortGetFreeHeapSize();
}

uint64_t esp_timer_get_time(void)
{
	/* Needed for iperf example.
	 * systems can use their hardware clocks for perfect clock.
	 * approximating here.
	 */
	return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

#if !USE_STD_C_LIB_MALLOC
/* Implement malloc etc hook so that standard c library will not be used */
void* malloc(size_t size)
{
    return MALLOC(size);
}

void* calloc(size_t n, size_t size)
{
    return _calloc_r(_REENT, n, size);
}

void* realloc(void* ptr, size_t size)
{
    return hosted_realloc(ptr, size);
}

void free(void *ptr)
{
    FREE(ptr);
}

void* _malloc_r(struct _reent *r, size_t size)
{
    return MALLOC(size);
}

void _free_r(struct _reent *r, void* ptr)
{
    FREE(ptr);
}

void* _realloc_r(struct _reent *r, void* ptr, size_t size)
{
    return hosted_realloc( ptr, size );
}

void* _calloc_r(struct _reent *r, size_t nmemb, size_t size)
{
    void *result;
    size_t size_bytes;
    if (__builtin_mul_overflow(nmemb, size, &size_bytes)) {
        return NULL;
    }
    result = MALLOC(size_bytes);
    if (result != NULL) {
        bzero(result, size_bytes);
    }
    return result;
}

/* No-op function, used to force linking this file,
   instead of the heap implementation from newlib.
 */
void newlib_include_heap_impl(void)
{
}

int malloc_trim(size_t pad)
{
    return 0; // indicates failure
}

size_t malloc_usable_size(void* p)
{
    return 0;
}

void malloc_stats(void)
{
}


int mallopt(int parameter_number, int parameter_value)
{
    return 0; // indicates failure
}

//void* valloc(size_t n) __attribute__((alias("malloc")));
//void* pvalloc(size_t n) __attribute__((alias("malloc")));
void cfree(void* p) __attribute__((alias("free")));
#endif

#if 0
ssize_t _write_r_console(struct _reent *r, int fd, const void * data, size_t size)
{
    const char* cdata = (const char*) data;
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        for (size_t i = 0; i < size; ++i) {
        	UART_TRANSMIT(cdata[i]);
        }
        return size;
    }
    __errno_r(r) = EBADF;
    return -1;
}

ssize_t _read_r_console(struct _reent *r, int fd, void * data, size_t size)
{
    char* cdata = (char*) data;
    if (fd == STDIN_FILENO) {
        size_t received;
        for (received = 0; received < size; ++received) {
            int status = UART_RECEIVE(cdata[received]);
            if (status != 0) {
                break;
            }
        }
        if (received == 0) {
            errno = EWOULDBLOCK;
            return -1;
        }
        return received;
    }
    __errno_r(r) = EBADF;
    return -1;
}
#endif

/**
  * @brief FreeRTOS hook function for idle task stack
  * @param  None
  * @retval None
  */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
	StackType_t **ppxIdleTaskStackBuffer,
	uint32_t *pulIdleTaskStackSize)
{
	/* If the buffers to be provided to the Idle task are declared
	 * inside this function then they must be declared static –
	 * otherwise they will be allocated on the stack and so not exists
	 * after this function exits.
	 * */
	static StaticTask_t xIdleTaskTCB;
	static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

	/* Pass out a pointer to the StaticTask_t structure in which the
	 * Idle task’s state will be stored. */
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

	/* Pass out the array that will be used as the Idle task’s stack. */
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;

	/* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/**
  * @brief FreeRTOS hook function for timer task stack
  * @param  None
  * @retval None
  */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
	StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
	/* USER CODE BEGIN GET_TIMER_TASK_MEMORY */
	static StaticTask_t xTimerTaskTCBBuffer;
	static StackType_t xTimerStack[CONFIG_TIMER_TASK_STACK_DEPTH];


	*ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
	*ppxTimerTaskStackBuffer = &xTimerStack[0];
	*pulTimerTaskStackSize = CONFIG_TIMER_TASK_STACK_DEPTH;
  /* place for user code */
}

void hosted_log_write(int level,
                   const char *tag,
                   const char *format, ...)
{
	esp_log_level_t level_for_tag = esp_log_level_get(tag);
	if ((ESP_LOG_NONE != level_for_tag) && (level <= level_for_tag)) {
		va_list list;
		va_start(list, format);
		vprintf(format, list);
		va_end(list);
	}
}

hosted_osi_funcs_t g_hosted_osi_funcs = {
	._h_memcpy                   =  hosted_memcpy                  ,
	._h_memset                   =  hosted_memset                  ,
	._h_malloc                   =  hosted_malloc                  ,
	._h_calloc                   =  hosted_calloc                  ,
	._h_free                     =  hosted_free                    ,
	._h_realloc                  =  hosted_realloc                 ,
	._h_thread_create            =  hosted_thread_create           ,
	._h_thread_cancel            =  hosted_thread_cancel           ,
	._h_msleep                   =  hosted_msleep                  ,
	._h_sleep                    =  hosted_sleep                   ,
	._h_blocking_delay           =  hosted_for_loop_delay          ,
	._h_queue_item               =  hosted_queue_item              ,
	._h_create_queue             =  hosted_create_queue            ,
	._h_dequeue_item             =  hosted_dequeue_item            ,
	._h_destroy_queue            =  hosted_destroy_queue           ,
	._h_reset_queue              =  hosted_reset_queue             ,
	._h_unlock_mutex             =  hosted_unlock_mutex            ,
	._h_create_mutex             =  hosted_create_mutex            ,
	._h_lock_mutex               =  hosted_lock_mutex              ,
	._h_destroy_mutex            =  hosted_destroy_mutex           ,
	._h_post_semaphore           =  hosted_post_semaphore          ,
	._h_post_semaphore_from_isr  =  hosted_post_semaphore_from_isr ,
	._h_create_semaphore         =  hosted_create_semaphore ,
	._h_get_semaphore            =  hosted_get_semaphore           ,
	._h_destroy_semaphore        =  hosted_destroy_semaphore       ,
	._h_timer_stop               =  hosted_timer_stop              ,
	._h_timer_start              =  hosted_timer_start             ,
#if H_USE_MEMPOOL
	._h_create_lock_mempool      =  hosted_create_lock_mempool     ,
	._h_lock_mempool             =  hosted_lock_mempool            ,
	._h_unlock_mempool           =  hosted_unlock_mempool          ,
#endif
    ._h_config_gpio              =  hosted_config_gpio             ,
    ._h_config_gpio_as_interrupt =  hosted_config_gpio_as_interrupt,
    ._h_read_gpio                =  hosted_read_gpio               ,
    ._h_write_gpio               =  hosted_write_gpio              ,
	._h_bus_init                 =  hosted_spi_init                ,
    ._h_do_bus_transfer          =  hosted_do_spi_transfer         ,
    ._h_event_wifi_post          =  hosted_wifi_event_post         ,
	._h_printf                   =  hosted_log_write               ,
	._h_hosted_init_hook         =  hosted_init_hook               ,
};

