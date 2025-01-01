/*
 * Copyright (c) 2006-2024 Evlers Developers
 *
 * Change Logs:
 * Date         Author      Notes
 * 2024-12-26   Evlers      first implementation
 */

#ifndef __OS_WRAPPER_H
#define __OS_WRAPPER_H

#include "os_header.h"
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

#include "mempool.h"

#include "esp_hosted_config.h"
#include "hosted_os_adapter.h"
#include "esp_wifi_types.h"
#include "esp_netif_types.h"
#include "common.h"

#define MAX_PAYLOAD_SIZE 							(MAX_TRANSPORT_BUFFER_SIZE-H_ESP_PAYLOAD_HEADER_OFFSET)


#define RPC__TIMER_ONESHOT                          (RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER)
#define RPC__TIMER_PERIODIC                         (RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER)

#define HOSTED_BLOCKING                             RT_WAITING_FOREVER
#define HOSTED_NON_BLOCKING                         0
#define HOSTED_BLOCK_MAX                            HOSTED_BLOCKING

#define thread_handle_t                            	rt_thread_t
#define queue_handle_t                              rt_mq_t
#define semaphore_handle_t                          rt_sem_t
#define mutex_handle_t                              rt_mutex_t
#define spinlock_handle_t                           struct rt_spinlock

#ifndef likely
#define likely(x)  									__builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) 								__builtin_expect(!!(x), 0)
#endif

#define FAST_RAM_ATTR

#define gpio_pin_state_t                            rt_base_t

#define RPC_TASK_STACK_SIZE                         ESP_HOSTED_RPC_THREAD_STACK_SIZE
#define RPC_TASK_PRIO                               ESP_HOSTED_RPC_THREAD_PRIORITY
#define DFLT_TASK_STACK_SIZE                        ESP_HOSTED_TRANSPORT_THREAD_STACK_SIZE
#define DFLT_TASK_PRIO                            	ESP_HOSTED_TRANSPORT_THREAD_PRIORITY



#define H_GPIO_MODE_DEF_DISABLE         			PIN_MODE_INPUT
#define H_GPIO_MODE_DEF_INPUT           			PIN_MODE_INPUT
#define H_GPIO_MODE_DEF_OUTPUT          			PIN_MODE_OUTPUT
#define H_GPIO_MODE_DEF_OD              			PIN_MODE_OUTPUT_OD

#define H_GPIO_INTR_POSEDGE							PIN_IRQ_MODE_RISING
#define H_GPIO_INTR_NEGEDGE							PIN_IRQ_MODE_FALLING
#define H_GPIO_INTR_ANYEDGE							PIN_IRQ_MODE_RISING_FALLING

#define H_GPIO_LOW                                   0
#define H_GPIO_HIGH                                  1


#define RET_OK                                     	0
#define RET_FAIL                                    -1
#define RET_INVALID                                 -2
#define RET_FAIL_MEM                                -3
#define RET_FAIL4                                   -4
#define RET_FAIL_TIMEOUT                            -5


#define MEM_DUMP(s)


/* -------- Create handle ------- */
#define HOSTED_CREATE_HANDLE(tYPE, hANDLE) {                                   \
	hANDLE = (tYPE *)g_h.funcs->_h_malloc(sizeof(tYPE));                       \
	if (!hANDLE) {                                                             \
		printf("%s:%u Mem alloc fail while create handle\n", __func__,__LINE__); \
		return NULL;                                                           \
	}                                                                          \
}

/* -------- Calloc, Free handle ------- */
#define HOSTED_FREE(buff) if (buff) { g_h.funcs->_h_free(buff); buff = NULL; }
#define HOSTED_CALLOC(struct_name, buff, nbytes, gotosym) do {    \
	buff = (struct_name *)g_h.funcs->_h_calloc(1, nbytes);	  \
	if (!buff) {                                                  \
		printf("%s, Failed to allocate memory \n", __func__);     \
		goto gotosym;                                             \
	}                                                             \
} while(0);

#define HOSTED_MALLOC(struct_name, buff, nbytes, gotosym) do {    \
	buff = (struct_name *)g_h.funcs->_h_malloc(nbytes);		  \
	if (!buff) {                                                  \
		printf("%s, Failed to allocate memory \n", __func__);     \
		goto gotosym;                                             \
	}                                                             \
} while(0);

#endif /*__OS_WRAPPER_H*/
