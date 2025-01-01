/*
 * SPDX-FileCopyrightText: 2019-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Example code to initialize STM Ethernet interface and connect to ESP-NetIF
 */

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_netif_net_stack.h"

#include "lwip/esp_pbuf_ref.h"
#include "lwip/esp_netif_net_stack.h"

#include "lwip/netif.h"
#include "lwip/err.h"
#include "lwip/etharp.h"
#include "lwip/ethip6.h"

#include "eth.h"
#include "stm32h7xx_hal.h"
#include "lan8742.h"

#include "hosted_os_adapter.h"

#include "stm_ethernetif.h"
#include "stm_eth_mempool.h"

#include "esp_log.h"
static const char *TAG = "stm-eth";

ESP_EVENT_DEFINE_BASE(ETH_EVENT);

// adjust as required
#define THREAD_PRIO 23
#define THREAD_STACK_SIZE 1024

/*
 * Refs:
 * - https://community.st.com/t5/stm32-mcus/how-to-create-a-project-for-stm32h7-with-ethernet-and-lwip-stack/ta-p/49308
 * - https://community.st.com/t5/stm32-mcus/how-to-create-a-bare-metal-hal-ethernet-application-on-stm32h563/ta-p/699164
 */

/*** ETH PHY Interface ***/

int32_t ETH_PHY_INTERFACE_Init(void);
int32_t ETH_PHY_INTERFACE_DeInit (void);
int32_t ETH_PHY_INTERFACE_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);
int32_t ETH_PHY_INTERFACE_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
int32_t ETH_PHY_INTERFACE_GetTick(void);

lan8742_Object_t LAN8742;
lan8742_IOCtx_t  LAN8742_IOCtx = {ETH_PHY_INTERFACE_Init,
		ETH_PHY_INTERFACE_DeInit,
		ETH_PHY_INTERFACE_WriteReg,
		ETH_PHY_INTERFACE_ReadReg,
		ETH_PHY_INTERFACE_GetTick};

int32_t ETH_PHY_INTERFACE_Init(void)
{
	LAN8742_RegisterBusIO(&LAN8742, &LAN8742_IOCtx);

	return 0;
}

int32_t ETH_PHY_Init(void)
{
	return 0;
}
int32_t ETH_PHY_INTERFACE_DeInit (void)
{
	return 0;
}

int32_t ETH_PHY_INTERFACE_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal)
{
	if(HAL_ETH_ReadPHYRegister(&heth, DevAddr, RegAddr, pRegVal) != HAL_OK) {
		return -1;
	}
	return 0;
}

int32_t ETH_PHY_INTERFACE_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal)
{
	if(HAL_ETH_WritePHYRegister(&heth, DevAddr, RegAddr, RegVal) != HAL_OK)
	{
		return -1;
	}
	return 0;
}

int32_t ETH_PHY_INTERFACE_GetTick(void)
{
	return HAL_GetTick();
}

int32_t ETH_PHY_INTERFACE_GetLinkState(void)
{
	return LAN8742_GetLinkState(&LAN8742);
}

int32_t ETH_PHY_INTERFACE_SetLinkUp(int32_t PHYLinkState)
{
	ETH_MACConfigTypeDef MACConf = {0};
	uint32_t duplex, speed = 0U;

	switch (PHYLinkState)
	{
	case LAN8742_STATUS_100MBITS_FULLDUPLEX:
		duplex = ETH_FULLDUPLEX_MODE;
		speed = ETH_SPEED_100M;
		break;
	case LAN8742_STATUS_100MBITS_HALFDUPLEX:
		duplex = ETH_HALFDUPLEX_MODE;
		speed = ETH_SPEED_100M;
		break;
	case LAN8742_STATUS_10MBITS_FULLDUPLEX:
		duplex = ETH_FULLDUPLEX_MODE;
		speed = ETH_SPEED_10M;
		break;
	case LAN8742_STATUS_10MBITS_HALFDUPLEX:
		duplex = ETH_HALFDUPLEX_MODE;
		speed = ETH_SPEED_10M;
		break;
	default:
		duplex = ETH_FULLDUPLEX_MODE;
		speed = ETH_SPEED_100M;
		break;
	}

	HAL_ETH_GetMACConfig(&heth, &MACConf);
	MACConf.DuplexMode = duplex;
	MACConf.Speed = speed;
	MACConf.DropTCPIPChecksumErrorPacket = DISABLE;
	MACConf.ForwardRxUndersizedGoodPacket = ENABLE;
	HAL_ETH_SetMACConfig(&heth, &MACConf);

	return LAN8742_STATUS_OK;
}

void ETH_StartLink(esp_netif_t *esp_netif, esp_ip6_addr_t *addr)
{
	struct netif *netif = esp_netif_get_netif_impl(esp_netif);

	int32_t PHYLinkState = 0U;

	/* Initialize the LAN8742 ETH PHY */
	if(LAN8742_Init(&LAN8742) != LAN8742_STATUS_OK) {
		ESP_LOGE(TAG, "failed to init LAN8742");
		esp_netif_action_disconnected(esp_netif, ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, 0);
		return;
	}

	PHYLinkState = ETH_PHY_INTERFACE_GetLinkState();

	if(PHYLinkState <= LAN8742_STATUS_LINK_DOWN) {
		ESP_LOGD(TAG, "%s link down", __func__);
	} else {
		ETH_PHY_INTERFACE_SetLinkUp(PHYLinkState);

#if CONFIG_LWIP_IPV6
		int8_t addr_index = 0;

		netif_ip6_addr_set(netif, addr_index, (ip6_addr_t *)addr);
		netif_ip6_addr_set_state(netif, addr_index, IP6_ADDR_VALID);
#endif
	}

	// set link as down. Thread to check link status will bring the link up if connected
	esp_netif_action_disconnected(esp_netif, ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, 0);

	/* start ethernet */
	HAL_ETH_Start_IT(&heth);
}

void * RxPktSemaphore = NULL; /* Semaphore to signal incoming packets */
void * TxPktSemaphore = NULL; /* Semaphore to signal transmit packet complete. Not used as we use blocking Tx call */

void * EthIfThread = NULL; /* Handle of the interface thread */
void * EthLinkThread = NULL; /* Handle of the interface link thread */

void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
	ESP_LOGD(TAG, "%s", __func__);

	ETH_AppBuff * p = stm_rx_mempool_get();
	if (p) {
		*buff = (uint8_t *)p + offsetof(ETH_AppBuff, buffer);
		p->AppBuff.next = NULL;
		p->AppBuff.len = ETH_RX_BUFFER_SIZE;
	} else {
		ESP_LOGE(TAG, "%s: mempool_get failed", __func__);
		*buff = NULL;
	}
}

void HAL_ETH_RxLinkCallback(void ** pStart, void ** pEnd, uint8_t * buff, uint16_t Length)
{
	ESP_LOGD(TAG, "%s", __func__);

	ETH_AppBuff ** ppStart = (ETH_AppBuff **) pStart;
	ETH_AppBuff ** ppEnd = (ETH_AppBuff **) pEnd;
	ETH_AppBuff * p = NULL;
	p = (ETH_AppBuff * )(buff - offsetof(ETH_AppBuff, buffer));
	p->AppBuff.next = NULL;
	p->AppBuff.len = Length;
	if (! *ppStart) {
		*ppStart = p;
	} else {
		(*ppEnd)->AppBuff.next = (ETH_BufferTypeDef *)p;
	}
	*ppEnd = p;

	/* Invalidate data cache because Rx DMA's writing to physical memory makes it stale. */
	SCB_InvalidateDCache_by_Addr((uint32_t *)buff, Length);
}

/**
  * @brief  Ethernet Rx Transfer completed callback
  * @param  heth: ETH handle
  * @retval None
  */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
	g_h.funcs->_h_post_semaphore_from_isr(RxPktSemaphore);
}

/**
  * @brief  Ethernet Tx Transfer completed callback
  * @param  heth: ETH handle
  * @retval None
  */
void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth)
{
	g_h.funcs->_h_post_semaphore_from_isr(TxPktSemaphore);
}

/**
  * @brief  Ethernet DMA transfer error callback
  * @param  heth: ETH handle
  * @retval None
  */
void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *heth)
{
	if((HAL_ETH_GetDMAError(heth) & ETH_DMACSR_RBU) == ETH_DMACSR_RBU) {
		g_h.funcs->_h_post_semaphore_from_isr(RxPktSemaphore);
	}

	if((HAL_ETH_GetDMAError(heth) & ETH_DMACSR_TBU) == ETH_DMACSR_TBU) {
		g_h.funcs->_h_post_semaphore_from_isr(TxPktSemaphore);
	}
}

/**************************************************************************************************/

// defined in stm32h7xx_hal_eth.c
extern ETH_TxPacketConfig TxConfig;

static esp_err_t stm_eth_transmit(void *driver, void *buffer, size_t len);
static esp_err_t stm_eth_post_attach(esp_netif_t *esp_netif, void *args);

// stm eth object, implements glue logic for stm_eth_driver and esp_netif
typedef struct {
	// ESP base netif driver
	esp_netif_driver_base_t base;
	esp_ip6_addr_t *ipv6_addr;
} stm_eth_obj_t;

static ETH_AppBuff *tx_buf = NULL;

static esp_err_t stm_eth_transmit(void *driver, void *buffer, size_t len)
{
	esp_err_t ret = ESP_OK;

	ESP_LOGD(TAG, "%s len %d", __func__, len);

	if (!tx_buf) {
		// allocate a buffer in the region which the ETH driver can access
		tx_buf = stm_rx_mempool_get();
		if (!tx_buf) {
			ESP_LOGE(TAG, "failed to alloc a buffer for Tx");
			return ESP_FAIL;
		}
	}
	ESP_LOGD(TAG, "tx_buf %p", tx_buf);

	// copy incoming data into buffer
	// done because buffer may from a location not accessible to ETH DMA
	memcpy(tx_buf->buffer, buffer, len);
	ESP_LOG_BUFFER_HEXDUMP("tx", tx_buf->buffer, 16, ESP_LOG_DEBUG);

	tx_buf->AppBuff.buffer = tx_buf->buffer;
	tx_buf->AppBuff.len = len;
	tx_buf->AppBuff.next = NULL;

	TxConfig.Length = len;
	TxConfig.TxBuffer = (ETH_BufferTypeDef *)tx_buf;
	TxConfig.pData = NULL;

	SCB_CleanDCache_by_Addr(tx_buf, sizeof(ETH_AppBuff));

	if(HAL_ETH_Transmit(&heth, &TxConfig, portMAX_DELAY) == HAL_OK) {
	// if(HAL_ETH_Transmit(&heth, &TxConfig, 0) == HAL_OK) {
		ret = ESP_OK;
	} else {
		ESP_LOGE(TAG, "HW failed to send packet");
		HAL_ETH_ReleaseTxPacket(&heth);
		ret = ESP_FAIL;
	}
	return ret;
}

static bool start_check_link_task = false;

static void stm_eth_check_link_task(const void * arg)
{
	int32_t PHYLinkState = 0U;
	bool link_is_up = false;

	ESP_LOGD(TAG, "%s", __func__);

	stm_eth_obj_t *obj = (stm_eth_obj_t *)arg;

	while (!start_check_link_task) {
		g_h.funcs->_h_sleep(1);
	}

	ESP_LOGD(TAG, "check link task started");

	while (true) {
		PHYLinkState = ETH_PHY_INTERFACE_GetLinkState();

		if (link_is_up) {
			// did link go down
			if(PHYLinkState <= LAN8742_STATUS_LINK_DOWN) {
				ESP_LOGD(TAG, "*** LINK is now DOWN ***");

				link_is_up = false;

				esp_netif_action_disconnected(obj->base.netif, ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, 0);
				esp_event_post(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY);
			}
		} else {
			// did link go up
			if(PHYLinkState > LAN8742_STATUS_LINK_DOWN) {
				ESP_LOGD(TAG, "*** LINK is now Up ***");

				ETH_PHY_INTERFACE_SetLinkUp(PHYLinkState);

				link_is_up = true;

				esp_netif_action_connected(obj->base.netif, ETH_EVENT, ETHERNET_EVENT_CONNECTED, 0);
				esp_event_post(ETH_EVENT, ETHERNET_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);
			}
		}

		g_h.funcs->_h_sleep(1);
	}
}

static void stm_eth_receive_task(const void * arg)
{
	ESP_LOGD(TAG, "%s", __func__);

	stm_eth_obj_t *obj = (stm_eth_obj_t *)arg;

	ETH_AppBuff *rxbuff;

	while (true) {
		// wait for Rx sem to be triggered
		g_h.funcs->_h_get_semaphore(RxPktSemaphore, portMAX_DELAY);
		// read incoming ETH data
		if (HAL_OK == HAL_ETH_ReadData(&heth, (void **)&rxbuff)) {
			ESP_LOGD(TAG, "rx %p, len: %d", rxbuff->buffer, rxbuff->AppBuff.len);
			ESP_LOG_BUFFER_HEXDUMP("eth", rxbuff->buffer, 14, ESP_LOG_DEBUG);
			esp_netif_receive(obj->base.netif, rxbuff->buffer, rxbuff->AppBuff.len, NULL);
		}
	}
}

/*
 * Buffer sent in esp_netif_receive() can now be freed
 */
static void stm_eth_free_rx_buffer(void *h, void * buffer)
{
	ESP_LOGD(TAG, "%s, %p", __func__, buffer);

	// release mempool entry based on the provided buffer
	stm_rx_mempool_release_buf_ref(buffer);
}

/** @brief Return list registration index of the supplied netif ptr
 */
static int get_esp_netif_index(esp_netif_t *esp_netif)
{
	esp_netif_t *netif = NULL;
	int counter = 0;
	// while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
	while ((netif = esp_netif_next(netif)) != NULL) {
		if (esp_netif == netif) {
			return counter;
		}
		counter++;
	}
	return -1;
}

static err_t ethernet_low_level_output(struct netif *netif, struct pbuf *p)
{
	ESP_LOGD(TAG, "%s", __func__);

	struct pbuf *q = p;
	esp_netif_t *esp_netif = esp_netif_get_handle_from_netif_impl(netif);
	esp_err_t ret = ESP_FAIL;

	if (!esp_netif) {
		ESP_LOGE(TAG, "corresponding esp-netif is NULL: netif=%p pbuf=%p len=%d\n", netif, p, p->len);
		return ERR_IF;
	}

	if (q->next == NULL) {
		ESP_LOGD(TAG, "tx len: %d", q->len);
		ESP_LOG_BUFFER_HEXDUMP("tx", q->payload, 16, ESP_LOG_DEBUG);
		ret = esp_netif_transmit(esp_netif, q->payload, q->len);
	} else {
		ESP_LOGW(TAG, "pbuf is a list, application may have a bug");
		q = pbuf_alloc(PBUF_RAW_TX, p->tot_len, PBUF_RAM);
		if (q != NULL) {
			pbuf_copy(q, p);
		} else {
			return ERR_MEM;
		}
		ret = esp_netif_transmit(esp_netif, q->payload, q->len);
		/* contents transmitted: should be safe to free pbuf */
		pbuf_free(q);
	}

	if (ret == ESP_OK) {
		return ERR_OK;
	} else if (ret == ESP_ERR_NO_MEM) {
		return ERR_MEM;
	}
	return ERR_IF;
}

static err_t esp_stm_eth_init(struct netif *netif)
{
	ESP_LOGI(TAG, "mac: %02x:%02x:%02x:%02x:%02x:%02x",
			heth.Init.MACAddr[0], heth.Init.MACAddr[1], heth.Init.MACAddr[2],
			heth.Init.MACAddr[3], heth.Init.MACAddr[4], heth.Init.MACAddr[5]);

	if (netif == NULL) {
		return ERR_IF;
	}
	esp_netif_t *esp_netif = netif->state;
	int esp_index = get_esp_netif_index(esp_netif);
	if (esp_index < 0) {
		return ERR_IF;
	}

	// Store netif index in net interface for open command to abstract the dev
	netif->state = (void *)esp_index;

	/* set MAC hardware address length */
	netif->hwaddr_len = ETH_HWADDR_LEN;

	/* set MAC hardware address */
	netif->hwaddr[0] =  heth.Init.MACAddr[0];
	netif->hwaddr[1] =  heth.Init.MACAddr[1];
	netif->hwaddr[2] =  heth.Init.MACAddr[2];
	netif->hwaddr[3] =  heth.Init.MACAddr[3];
	netif->hwaddr[4] =  heth.Init.MACAddr[4];
	netif->hwaddr[5] =  heth.Init.MACAddr[5];

	/* maximum transfer unit */
	netif->mtu = 1500;

	/* device capabilities */
	/* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
	// netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHERNET;

#if ESP_LWIP
#if LWIP_IGMP
	netif->flags |= NETIF_FLAG_IGMP;
#endif
#endif

#if ESP_IPV6
#if LWIP_IPV6 && LWIP_IPV6_MLD
	netif->flags |= NETIF_FLAG_MLD6;
#endif
#endif

	// follow what esp components/esp_netif/lwip/netif/ethernetif.c->ethernetif_init() does
	netif->hostname = "lwip";
	netif->name[0] = 'e';
	netif->name[1] = 't';
#if LWIP_IPV4
	netif->output = etharp_output;
#endif
#if LWIP_IPV6
	netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */
	netif->linkoutput = ethernet_low_level_output;

	return ERR_OK;
}

esp_netif_recv_ret_t esp_netif_lwip_stm_eth_input(void *h, void *buffer, unsigned int len, void *eb)
{
	ESP_LOGD(TAG, "%s %p %d", __func__, buffer, len);
	ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, len, ESP_LOG_DEBUG);

	struct netif *netif = h;
	esp_netif_t *esp_netif = esp_netif_get_handle_from_netif_impl(netif);
	struct pbuf *p;

	if (!netif_is_up(netif)) {
		ESP_LOGE(TAG, "netif is NOT up");
		if (buffer) {
			esp_netif_free_rx_buffer(esp_netif, buffer);
		}
		return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_FAIL);
	}

	/* allocate custom pbuf to hold  */
	p = esp_pbuf_allocate(esp_netif, buffer, len, buffer);
	if (p == NULL) {
		esp_netif_free_rx_buffer(esp_netif, buffer);
		return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_ERR_NO_MEM);
	}
	/* full packet send to tcpip_thread to process */
	if (netif->input(p, netif) != ERR_OK) {
		ESP_LOGE(TAG, "ip input error");
		pbuf_free(p);
		return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_FAIL);
	}

	return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_OK);
}

/*
 * Functions called by esp_netif to init netif and send incoming data to netif
 */
const struct esp_netif_netstack_config s_netif_config_stm_eth = {
	.lwip = {
		.init_fn = esp_stm_eth_init,
		.input_fn = esp_netif_lwip_stm_eth_input,
	}
};

/*
 * Starts the driver and required semaphores and tasks
 */
static esp_err_t stm_eth_driver_start(stm_eth_obj_t *obj)
{
	ESP_LOGD(TAG, "%s", __func__);

	// init PHY
	if (ETH_PHY_INTERFACE_Init()) {
		return ESP_FAIL;
	}

	/* create a binary semaphore used for informing ethernetif of frame reception */
	RxPktSemaphore = g_h.funcs->_h_create_semaphore(1);
	g_h.funcs->_h_get_semaphore(RxPktSemaphore, portMAX_DELAY);

	/* create a binary semaphore used for informing ethernetif of frame transmission */
	TxPktSemaphore = g_h.funcs->_h_create_semaphore(1);
	g_h.funcs->_h_get_semaphore(TxPktSemaphore, portMAX_DELAY);

	/* create the task that handles the ETH_MAC */
	EthIfThread = g_h.funcs->_h_thread_create("stm_eth_receive", THREAD_PRIO, THREAD_STACK_SIZE, stm_eth_receive_task, obj);

	/* create the task that handles link checking */
	EthLinkThread = g_h.funcs->_h_thread_create("stm_eth_link", THREAD_PRIO, THREAD_STACK_SIZE, stm_eth_check_link_task, obj);

	ETH_StartLink(obj->base.netif, obj->ipv6_addr);

	return ESP_OK;
}

/* Post-attach callback
 * This part initializes the lower level driver
 * also provides the functions needed by esp_netif to tx a packet
 * and to free a rx buffer (no longer needed by netif)
 */
static esp_err_t stm_eth_post_attach(esp_netif_t *esp_netif, void *args)
{
	// int32_t ret;

	ESP_LOGD(TAG, "%s", __func__);

	stm_eth_obj_t *obj = (stm_eth_obj_t *)args;

	const esp_netif_driver_ifconfig_t driver_ifconfig = {
		.transmit = stm_eth_transmit,
		.handle = obj,
		.driver_free_rx_buffer = stm_eth_free_rx_buffer,
	};

	obj->base.netif = esp_netif;
	ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));

	stm_eth_driver_start(obj); // starts the driver and background tasks (rx, link status)

	start_check_link_task = true; // triggers the task used to check link status

	// tells esp_netif to start
	esp_netif_action_start(esp_netif, ETH_EVENT, ETHERNET_EVENT_START, 0);
	esp_event_post(ETH_EVENT, ETHERNET_EVENT_START, NULL, 0, portMAX_DELAY);

	return ESP_OK;
}

static esp_ip6_addr_t local_addr;        /* Local IP6 address */

// create a new eth obj
stm_eth_obj_t * stm_eth_obj_create(esp_netif_t *stm_eth_netif)
{
	ESP_LOGD(TAG, "%s", __func__);

	IP6_ADDR(&local_addr,
			lwip_htonl(0xfd0000),
			lwip_htonl(0x00000000),
			lwip_htonl(0x00000000),
			lwip_htonl(0x00000001)
	);

	if (!stm_eth_netif) {
		ESP_LOGE(TAG, "invalid netif param");
		return NULL;
	}

	stm_eth_obj_t *obj = g_h.funcs->_h_calloc(1, sizeof(stm_eth_obj_t));
	if (!obj) {
		ESP_LOGE(TAG, "failed to create stm_eth_obj");
		return NULL;
	}

	// Attach driver and post_attach callback
	obj->base.post_attach = stm_eth_post_attach;
	obj->base.netif = stm_eth_netif;
	obj->ipv6_addr = &local_addr;

	return obj;
}

/**************************************************************************************************/

#ifdef CONFIG_LWIP_IPV6_AUTOCONFIG
#define ESP_NETIF_DEFAULT_IPV6_AUTOCONFIG_FLAGS (ESP_NETIF_FLAG_IPV6_AUTOCONFIG_ENABLED)
#else
#define ESP_NETIF_DEFAULT_IPV6_AUTOCONFIG_FLAGS (0)
#endif

// copied from ESP_NETIF_INHERENT_DEFAULT_ETH()
#define ESP_NETIF_INHERENT_DEFAULT_STM_ETH() \
{   \
	.flags = (esp_netif_flags_t)(ESP_NETIF_IPV4_ONLY_FLAGS(ESP_NETIF_DHCP_CLIENT) | ESP_NETIF_DEFAULT_ARP_FLAGS | ESP_NETIF_FLAG_EVENT_IP_MODIFIED | ESP_NETIF_DEFAULT_IPV6_AUTOCONFIG_FLAGS), \
	ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(mac) \
	ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(ip_info) \
	.get_ip_event = IP_EVENT_ETH_GOT_IP, \
	.lost_ip_event = IP_EVENT_ETH_LOST_IP, \
	.if_key = "ETH_DEF",     \
	.if_desc = "eth",        \
	.route_prio = 50,        \
	.bridge_info = NULL      \
};

esp_netif_t * stm_eth_if_init(void)
{
	ESP_LOGD(TAG, "%s", __func__);

	stm_rx_mempool_init();

	esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_STM_ETH();

	esp_netif_config_t cfg = {
			.base = &base_cfg,
			.driver = NULL,
			.stack = &s_netif_config_stm_eth,
	};
	esp_netif_t *stm_eth_netif = esp_netif_new(&cfg);
	void *obj = stm_eth_obj_create(stm_eth_netif);
	assert(obj);
	// attach eth to esp_netif
	ESP_ERROR_CHECK(esp_netif_attach(stm_eth_netif, obj));

	return stm_eth_netif;
}
