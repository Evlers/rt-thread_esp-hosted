/*
 * Copyright (c) 2006-2024 Evlers Developers
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date         Author      Notes
 * 2024-01-04   Evlers      first implementation
 */

#include <stdint.h>
#include <string.h>
#include "rtthread.h"
#include "rtdevice.h"
#include "ctrl_api.h"
#include "netdev_api.h"
#include "common/util.h"
#include "transport_drv.h"

#define DBG_TAG           "esp.wlan"
#define DBG_LVL           DBG_INFO
#include "rtdbg.h"


struct event_callback_table
{
    int event;
    ctrl_resp_cb_t fun;
};

struct drv_wifi
{
    struct rt_wlan_device *wlan;
    struct network_handle *net;
};

static rt_sem_t sem_esp_init;
static struct drv_wifi wifi_sta, wifi_ap;


/* espressif esp-hosted interface */

static void ctrl_cmd_default_req (ctrl_cmd_t *req)
{
    memset(req, 0, sizeof(ctrl_cmd_t));
    req->msg_type = CTRL_REQ;
    req->ctrl_resp_cb = NULL;
    req->cmd_timeout_sec = DEFAULT_CTRL_RESP_TIMEOUT;
}

static void free_ctrl_resp_msg (ctrl_cmd_t *resp)
{
    if (resp == NULL) return ;

    if (resp->free_buffer_handle != NULL && resp->free_buffer_func != NULL)
    {
        resp->free_buffer_func(resp->free_buffer_handle);
        resp->free_buffer_handle = NULL;
    }

    free(resp);
}

static int get_response_result(ctrl_cmd_t * resp)
{
    int ret = SUCCESS;

    if (!resp || (resp->msg_type != CTRL_RESP))
    {
        if (resp)
        {
            LOG_E("Msg type is not response[%u]", resp->msg_type);
        }
        ret = FAILURE;
        goto __exit;
    }

    if ((resp->msg_id <= CTRL_RESP_BASE) || (resp->msg_id >= CTRL_RESP_MAX))
    {
        LOG_E("Response Msg ID[%u] is not correct",resp->msg_id);
        ret = FAILURE;
        goto __exit;
    }

    if (resp->resp_event_status != SUCCESS) {
        LOG_E("Error reported: %08X", resp->resp_event_status);
        ret = FAILURE;
        goto __exit;
    }

    __exit:
    return ret;
}

static int sync_response_result(ctrl_cmd_t *resp)
{
    int ret = get_response_result(resp);
    free_ctrl_resp_msg(resp);
    return ret;
}

static int esp_ctrl_event_callback (ctrl_cmd_t * event)
{
    if (!event || (event->msg_type != CTRL_EVENT))
    {
        if (event)
            LOG_E("Msg type is not event[%u]",event->msg_type);
        goto fail_parsing;
    }

    if ((event->msg_id <= CTRL_EVENT_BASE) || (event->msg_id >= CTRL_EVENT_MAX))
    {
        LOG_E("Event Msg ID[%u] is not correct",event->msg_id);
        goto fail_parsing;
    }

    switch(event->msg_id)
    {
        case CTRL_EVENT_ESP_INIT:
            LOG_D("App EVENT: ESP INIT");
            rt_sem_release(sem_esp_init);
            break;

        case CTRL_EVENT_HEARTBEAT:
            LOG_D("App EVENT: Heartbeat event [%lu]", event->u.e_heartbeat.hb_num);
            break;

        case CTRL_EVENT_STATION_DISCONNECT_FROM_AP:
            LOG_D("App EVENT: Station mode: Disconnect Reason[%u]", event->resp_event_status);
            rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_DISCONNECT, RT_NULL);
            break;

        case CTRL_EVENT_STATION_DISCONNECT_FROM_ESP_SOFTAP:
        {
            char *p = event->u.e_sta_disconn.bssid;
            if (p && strlen(p)) {
                LOG_D("App EVENT: SoftAP mode: Disconnect MAC[%s]", p);
            }
        }
            break;

        default:
            LOG_D("Invalid event[%u] to parse", event->msg_id);
            break;
    }
    free_ctrl_resp_msg(event);
    return SUCCESS;

fail_parsing:
    free_ctrl_resp_msg(event);
    return FAILURE;
}

static int register_esp_event_callbacks(void)
{
    int ret = SUCCESS;

    struct event_callback_table events[] =
    {
        { CTRL_EVENT_ESP_INIT,                           esp_ctrl_event_callback },
        { CTRL_EVENT_HEARTBEAT,                          esp_ctrl_event_callback },
        { CTRL_EVENT_STATION_DISCONNECT_FROM_AP,         esp_ctrl_event_callback },
        { CTRL_EVENT_STATION_DISCONNECT_FROM_ESP_SOFTAP, esp_ctrl_event_callback },
    };

    for (uint8_t i = 0; i < sizeof(events) / sizeof(struct event_callback_table); i ++)
    {
        if (CALLBACK_SET_SUCCESS != set_event_callback(events[i].event, events[i].fun) )
        {
            LOG_E("event callback register failed for event[%u]", events[i].event);
            ret = FAILURE;
            break;
        }
    }
    return ret;
}

/**
  * @brief  transport driver event handler callback
  * @param  event - spi_drv_events_e event to be handled
  * @retval None
  */
static void transport_driver_event_handler(uint8_t event)
{
	static bool is_esp_init_done = false;

    switch (event)
    {
        case TRANSPORT_ACTIVE:
			if (is_esp_init_done)
			{
				LOG_W("esp network device restarts abnormally");
				rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_DISCONNECT, RT_NULL);
				rt_wlan_dev_indicate_event_handle(wifi_ap.wlan, RT_WLAN_DEV_EVT_AP_STOP, RT_NULL);
				break;
			}

            /* initiate control path now */
            if (init_hosted_control_lib())
            {
                LOG_E("Init hosted control lib failed");
                break;
            }

            LOG_D("Base transport is set-up");

            register_esp_event_callbacks();
			is_esp_init_done = true;
            break;

        default:
            break;
    }
}


/* RT-Thread wlan framework interface */

rt_inline struct drv_wifi *get_drv_wifi(struct rt_wlan_device *wlan)
{
    if (wlan == wifi_sta.wlan)
    {
        return &wifi_sta;
    }

    if (wlan == wifi_ap.wlan)
    {
        return &wifi_ap;
    }

    return RT_NULL;
}

static rt_wlan_security_t security_esp_to_wlan (int encryption_mode)
{
    switch (encryption_mode)
    {
        case WIFI_AUTH_OPEN:            return SECURITY_OPEN;
        case WIFI_AUTH_WEP:             return SECURITY_WEP_PSK;
        case WIFI_AUTH_WPA_PSK:         return SECURITY_WPA_AES_PSK;
        case WIFI_AUTH_WPA2_PSK:        return SECURITY_WPA2_AES_PSK;
        case WIFI_AUTH_WPA_WPA2_PSK:    return SECURITY_WPA2_AES_PSK;
        case WIFI_AUTH_WPA2_WPA3_PSK:   return SECURITY_WPA2_AES_PSK;
        default:                        return SECURITY_UNKNOWN;
    }
}

static int security_wlan_to_esp (rt_wlan_security_t security)
{
    switch (security)
    {
        case SECURITY_OPEN:         return WIFI_AUTH_OPEN;
        case SECURITY_WEP_PSK:      return WIFI_AUTH_WEP;
        case SECURITY_WPA_AES_PSK:  return WIFI_AUTH_WPA_PSK;
        case SECURITY_WPA2_AES_PSK: return WIFI_AUTH_WPA2_PSK;
        default:                    return WIFI_AUTH_WPA2_PSK;
    }
}

/* estimate the maximum rate for ap
 *  MIMO    Channel Width   Protocol        rate
 *  -           22MHz       802.11b         11Mbps
 *  -           20MHz       802.11g         54 Mbps
 *  1x1         20MHz       802.11n         72Mbps
 *  1x1         40MHz       802.11n         150Mbps
 *  2x2         20MHz       802.11n         144Mbps
 *  2x2         40MHz       802.11n         300Mbps
*/
static int estimate_ap_max_rate(wifi_scanlist_t *ap_info)
{
    int max_rate = 0;

    /* Check whether 802.11n (Wi-Fi 4) is supported */
    if (ap_info->support.phy_11n)
    {
        /* Since esp32 does not provide an API to obtain AP MIMO information, 
         * and esp32 only does not support MIMO,
         * our evaluated rate is also based on 1x1.
         */

        /* 20 MHz */
        if (ap_info->bandwidth == WIFI_BW_HT20)
        {
            max_rate = 72;
        }
        /* 40 MHz */
        else if (ap_info->bandwidth == WIFI_BW_HT40)
        {
            max_rate = 150;
        }
    }
    /* Check whether 802.11g is supported */
    else if (ap_info->support.phy_11g)
    {
        max_rate = 54;
    }
    /* Check whether 802.11b is supported */
    else if (ap_info->support.phy_11b)
    {
        max_rate = 11;
    }

    return max_rate;
}

static rt_err_t drv_wlan_init(struct rt_wlan_device *wlan)
{
    return RT_EOK;
}

static rt_err_t drv_wlan_mode(struct rt_wlan_device *wlan, rt_wlan_mode_t mode)
{
    return RT_EOK;
}

static int esp_scan_callback(ctrl_cmd_t * resp)
{
    if (get_response_result(resp) != RT_EOK)
    {
        LOG_E("wifi scan failed!");
        goto __finish_resp;
    }

    if (resp->msg_id == CTRL_RESP_GET_AP_SCAN_LIST)
    {
        wifi_ap_scan_list_t * w_scan_p = &resp->u.wifi_ap_scan;
        wifi_scanlist_t *list = w_scan_p->out_list;

        if (!w_scan_p->count)
        {
            LOG_D("No AP found");
            goto __finish_resp;
        }

        if (!list)
        {
            LOG_E("Failed to get scanned AP list");
            goto __fail_resp;
        }
        else
        {
            LOG_D("Number of available APs is %d", w_scan_p->count);

            for (uint16_t i = 0; i < w_scan_p->count; i ++)
            {
                struct rt_wlan_info wlan_info;
                struct rt_wlan_buff buff;

                rt_memset(&wlan_info, 0, sizeof(wlan_info));

                convert_mac_to_bytes(wlan_info.bssid, (const char *)list[i].bssid);
                wlan_info.ssid.len = min(strlen((const char *)list[i].ssid), sizeof(wlan_info.ssid.val));
                rt_memcpy(wlan_info.ssid.val, list[i].ssid, wlan_info.ssid.len);

                if (wlan_info.ssid.len)
                    wlan_info.hidden = 0;
                else
                    wlan_info.hidden = 1;

                wlan_info.channel = (rt_int16_t)list[i].channel;
                wlan_info.rssi = list[i].rssi;

                wlan_info.datarate = estimate_ap_max_rate(&list[i]) * 1000000;
                wlan_info.band = RT_802_11_BAND_UNKNOWN;

                wlan_info.security = security_esp_to_wlan(list[i].encryption_mode);

                buff.data = &wlan_info;
                buff.len = sizeof(wlan_info);

                rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_SCAN_REPORT, &buff);

                LOG_D("%d) ssid \"%s\" bssid \"%s\" rssi \"%d\" channel \"%d\" auth mode \"%d\" ",\
                        i, list[i].ssid, list[i].bssid, list[i].rssi,
                        list[i].channel, list[i].encryption_mode);
            }

            rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_SCAN_DONE, RT_NULL);
        }
    }
    else
    {
        goto __fail_resp;
    }

    __finish_resp:
    rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_SCAN_DONE, RT_NULL);
    free_ctrl_resp_msg(resp);
    return SUCCESS;

    __fail_resp:
    rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_SCAN_DONE, RT_NULL);
    free_ctrl_resp_msg(resp);
    return FAILURE;
}

static rt_err_t drv_wlan_scan(struct rt_wlan_device *wlan, struct rt_scan_info *scan_info)
{
    ctrl_cmd_t req;

    ctrl_cmd_default_req(&req);
    req.cmd_timeout_sec = 300;
    req.ctrl_resp_cb = esp_scan_callback;

    wifi_ap_scan_list(req);

    return RT_EOK;
}

static int esp_join_callback(ctrl_cmd_t * resp)
{
    rt_err_t ret;

    if ((ret = get_response_result(resp)) != SUCCESS)
    {
        LOG_E("wifi scan failed!");
        goto __exit;
    }

    if (resp->msg_id == CTRL_RESP_CONNECT_AP)
    {
        LOG_D("Connected");
        rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_CONNECT, RT_NULL);
    }

    __exit:
    if (ret != SUCCESS)
    {
        rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_CONNECT_FAIL, RT_NULL);
    }
    free_ctrl_resp_msg(resp);
    return ret;
}

static rt_err_t drv_wlan_join(struct rt_wlan_device *wlan, struct rt_sta_info *sta_info)
{
    ctrl_cmd_t req;

    ctrl_cmd_default_req(&req);
    memcpy(req.u.wifi_ap_config.ssid, sta_info->ssid.val, min(sta_info->ssid.len, sizeof(req.u.wifi_ap_config.ssid)));
    memcpy(req.u.wifi_ap_config.pwd, sta_info->key.val, min(sta_info->key.len, sizeof(req.u.wifi_ap_config.pwd)));
    req.u.wifi_ap_config.is_wpa3_supported = false;
    req.u.wifi_ap_config.listen_interval = 3; // default

    /* register callback for handling reply asynch-ly */
    req.ctrl_resp_cb = esp_join_callback;

    wifi_connect_ap(req);

    return RT_EOK;
}

static rt_err_t drv_wlan_softap(struct rt_wlan_device *wlan, struct rt_ap_info *ap_info)
{
    ctrl_cmd_t req;
    ctrl_cmd_t *resp = NULL;

    ctrl_cmd_default_req(&req);
    strncpy((char *)req.u.wifi_softap_config.ssid, (char *)ap_info->ssid.val, min(ap_info->ssid.len, SSID_LENGTH-1));
    strncpy((char *)req.u.wifi_softap_config.pwd, (char *)ap_info->key.val, min(ap_info->key.len, PASSWORD_LENGTH-1));
    req.u.wifi_softap_config.channel = ap_info->channel;
    req.u.wifi_softap_config.encryption_mode = security_wlan_to_esp(ap_info->security);
    req.u.wifi_softap_config.max_connections = 2;
    req.u.wifi_softap_config.ssid_hidden = ap_info->hidden;
    req.u.wifi_softap_config.bandwidth = WIFI_BW_HT40;

    resp = wifi_start_softap(req);

    if (sync_response_result(resp) == SUCCESS)
    {
        rt_wlan_dev_indicate_event_handle(wifi_ap.wlan, RT_WLAN_DEV_EVT_AP_START, RT_NULL);
    }
    else
    {
        rt_wlan_dev_indicate_event_handle(wifi_ap.wlan, RT_WLAN_DEV_EVT_AP_STOP, RT_NULL);
    }

    return RT_EOK;
}

static rt_err_t drv_wlan_disconnect(struct rt_wlan_device *wlan)
{
    ctrl_cmd_t req;
    ctrl_cmd_t *resp = NULL;

    ctrl_cmd_default_req(&req);
    resp = wifi_disconnect_ap(req);

    if (sync_response_result(resp) == SUCCESS)
    {
        rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_DISCONNECT, RT_NULL);
    }
    return RT_EOK;
}

static rt_err_t drv_wlan_ap_stop(struct rt_wlan_device *wlan)
{
    ctrl_cmd_t req;
    ctrl_cmd_t *resp = NULL;

    ctrl_cmd_default_req(&req);
    resp = wifi_stop_softap(req);

    if (sync_response_result(resp) == SUCCESS)
    {
        rt_wlan_dev_indicate_event_handle(wifi_ap.wlan, RT_WLAN_DEV_EVT_AP_STOP, RT_NULL);
    }

    return RT_EOK;
}

static rt_err_t drv_wlan_set_mac(struct rt_wlan_device *wlan, rt_uint8_t mac[])
{
    ctrl_cmd_t req;
    ctrl_cmd_t *resp = NULL;

    ctrl_cmd_default_req(&req);
    req.u.wifi_mac.mode = (wlan == wifi_sta.wlan) ? WIFI_MODE_STA : WIFI_MODE_AP;
    snprintf(req.u.wifi_mac.mac, sizeof(req.u.wifi_mac.mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    resp = wifi_set_mac(req);

    return sync_response_result(resp);
}

static rt_err_t drv_wlan_get_mac(struct rt_wlan_device *wlan, rt_uint8_t mac[])
{
    ctrl_cmd_t req;
    ctrl_cmd_t *resp = NULL;

    ctrl_cmd_default_req(&req);
    req.u.wifi_mac.mode = (wlan == wifi_sta.wlan) ? WIFI_MODE_STA : WIFI_MODE_AP;
    resp = wifi_get_mac(req);

    if (resp && SUCCESS == resp->resp_event_status)
    {
        if(!strlen(resp->u.wifi_mac.mac))
        {
            LOG_W("NULL MAC returned");
        }

        if (convert_mac_to_bytes(mac, resp->u.wifi_mac.mac) != SUCCESS)
        {
            LOG_W("convert mac to bytes failed");
        }
    }

    return sync_response_result(resp);
}

static int drv_wlan_send(struct rt_wlan_device *wlan, void *buff, int len)
{
    struct pbuf *tx_buffer = NULL;

    if (get_drv_wifi(wlan)->net == NULL)
    {
        return -RT_ERROR;
    }

    tx_buffer = malloc(sizeof(struct pbuf));
    RT_ASSERT(tx_buffer);

    tx_buffer->payload = malloc(len);
    RT_ASSERT(tx_buffer->payload);

    memcpy(tx_buffer->payload, buff, len);
    tx_buffer->len = len;

    if (network_write(get_drv_wifi(wlan)->net, tx_buffer) != SUCCESS)
    {
        LOG_E("Failed to send network data");
        free(tx_buffer);
    }

    return RT_EOK;
}

static const struct rt_wlan_dev_ops ops =
{
    .wlan_init = drv_wlan_init,
    .wlan_mode = drv_wlan_mode,
    .wlan_scan = drv_wlan_scan,
    .wlan_join = drv_wlan_join,
    .wlan_softap = drv_wlan_softap,
    .wlan_disconnect = drv_wlan_disconnect,
    .wlan_ap_stop = drv_wlan_ap_stop,
    .wlan_ap_deauth = NULL,
    .wlan_scan_stop = NULL,
    .wlan_get_rssi = NULL,
    .wlan_set_powersave = NULL,
    .wlan_get_powersave = NULL,
    .wlan_cfg_promisc = NULL,
    .wlan_cfg_filter = NULL,
    .wlan_cfg_mgnt_filter = NULL,
    .wlan_set_channel = NULL,
    .wlan_get_channel = NULL,
    .wlan_set_country = NULL,
    .wlan_get_country = NULL,
    .wlan_set_mac = drv_wlan_set_mac,
    .wlan_get_mac = drv_wlan_get_mac,
    .wlan_recv = NULL,
    .wlan_send = drv_wlan_send,
};

/**
  * @brief network data receive callback
  * @param  net_handle - network handle
  * @retval None
  */
static void network_data_rx_callback(struct network_handle *net_handle)
{
    struct pbuf *rx_buffer = NULL;
    struct rt_wlan_device *wlan = wifi_ap.net == net_handle ? wifi_ap.wlan : wifi_sta.wlan;

    rx_buffer = network_read(net_handle, 0);

    if (rx_buffer)
    {
        rt_wlan_dev_report_data(wlan, rx_buffer->payload, rx_buffer->len);

        free(rx_buffer->payload);
        rx_buffer->payload = NULL;
        free(rx_buffer);
        rx_buffer = NULL;
    }
}

int rt_hw_esp_wlan_init (void)
{
    static struct rt_wlan_device wlan_ap, wlan_sta;

    sem_esp_init = rt_sem_create("esp_init", 0, RT_IPC_FLAG_PRIO);
    RT_ASSERT(sem_esp_init != NULL);

    /* init network interface */
    network_init();

    /* init spi driver */
    transport_init(transport_driver_event_handler);

    /* wait for WiFi initialization */
    if (rt_sem_take(sem_esp_init, rt_tick_from_millisecond(10 * 1000)))
    {
        LOG_E("Wait for WiFi initialization timeout!");
        return -RT_ERROR;
    }

    /* register the network interface callback */
    wifi_ap.net = network_open(SOFTAP_INTERFACE, network_data_rx_callback);
    wifi_sta.net = network_open(STA_INTERFACE, network_data_rx_callback);

    /* register the wlan device and set its working mode */
    wifi_ap.wlan = &wlan_ap;
    wifi_sta.wlan = &wlan_sta;

    /* register wlan device for ap */
    if (rt_wlan_dev_register(&wlan_ap, RT_WLAN_DEVICE_AP_NAME, &ops, 0, &wifi_ap) != RT_EOK)
    {
        LOG_E("Failed to register a wlan_ap device!");
        return -RT_ERROR;
    }

    /* register wlan device for sta */
    if (rt_wlan_dev_register(&wlan_sta, RT_WLAN_DEVICE_STA_NAME, &ops, 0, &wifi_sta) != RT_EOK)
    {
        LOG_E("Failed to register a wlan_sta device!");
        return -RT_ERROR;
    }

    /* set wlan_sta to STATION mode */
    if (rt_wlan_set_mode(RT_WLAN_DEVICE_STA_NAME, RT_WLAN_STATION) != RT_EOK)
    {
        LOG_E("Failed to set %s to station mode!", RT_WLAN_DEVICE_STA_NAME);
        return -RT_ERROR;
    }

    /* set wlan_ap to AP mode */
    if (rt_wlan_set_mode(RT_WLAN_DEVICE_AP_NAME, RT_WLAN_AP) != RT_EOK)
    {
        LOG_E("Failed to set %s to ap mode!", RT_WLAN_DEVICE_AP_NAME);
        return -RT_ERROR;
    }

    return RT_EOK;
}

static void esp_hosted_init (void *parameter)
{
    /* reset esp32 chip */
    rt_pin_mode(ESP_HOSTED_RESET_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(ESP_HOSTED_RESET_PIN, PIN_LOW);
    rt_thread_mdelay(50);
    rt_pin_write(ESP_HOSTED_RESET_PIN, PIN_HIGH);

    /* stop spi transactions short time to avoid slave sync issues */
    rt_thread_mdelay(50);

    /* initialize the esp-hosted */
    rt_hw_esp_wlan_init();
}

static int esp_device_init (void)
{
#ifdef ESP_HOSTED_THREAD_INIT
    /* use thread initialization */
    rt_thread_t init_thread = rt_thread_create("esp_init", esp_hosted_init, NULL,
                                                ESP_HOSTED_INIT_THREAD_STACK_SIZE, ESP_HOSTED_INIT_THREAD_PRIORITY, 20);
    RT_ASSERT(init_thread != NULL);
    rt_thread_startup(init_thread);
#else
    /* thread initialization is not used */
    esp_hosted_init(NULL);
#endif

    return RT_EOK;
}
INIT_ENV_EXPORT(esp_device_init);
