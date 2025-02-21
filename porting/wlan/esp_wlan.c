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

#include <stdint.h>
#include <string.h>
#include "rtthread.h"
#include "rtdevice.h"

#include "wifi.h"
#include "esp_wifi.h"
#include "esp_hosted_api.h"

#define DBG_TAG             "esp.wlan"
#define DBG_LVL             DBG_INFO
#include "rtdbg.h"

struct drv_wifi
{
    struct rt_wlan_device *wlan;
};

static struct drv_wifi wifi_sta, wifi_ap;


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

/**
 * @brief The maximum rate is calculated based on the protocol and bandwidth
 * 
 * @param protocol_bit_map The protocol bit map is a bit map of the following:
 *                              WIFI_PROTOCOL_11B - 802.11b protocol
 *                              WIFI_PROTOCOL_11G - 802.11g protocol
 *                              WIFI_PROTOCOL_11N - 802.11n protocol
 *                              WIFI_PROTOCOL_LR - Low Rate protocol
 *                              WIFI_PROTOCOL_11A - 802.11a protocol
 *                              WIFI_PROTOCOL_11AC - 802.11ac protocol
 *                              WIFI_PROTOCOL_11AX - 802.11ax protocol
 * @param bandwidth Wi-Fi bandwidth type
 * @return int data rate(Kbps)
 */
static int compute_max_rate(uint8_t protocol_bit_map, wifi_bandwidth_t bandwidth)
{
    int max_rate = 0;

    if (protocol_bit_map & (WIFI_PROTOCOL_11AC | WIFI_PROTOCOL_11AX | WIFI_PROTOCOL_11N))
    {
        /* It's just a simple calculation
         * Assume minimum GI guard time and maximum Coding Rate
         * Since esp32 does not provide an API to obtain AP MIMO information, 
         * and esp32 only does not support MIMO,
         * our evaluated rate is also based on 1x1.
         */
        int spatial_streams = 1;

        if (protocol_bit_map & WIFI_PROTOCOL_11AX)
        {
            switch (bandwidth)
            {
                case WIFI_BW20: max_rate = 143400; break;
                case WIFI_BW40: max_rate = 286800; break;
                case WIFI_BW80: max_rate = 600500; break;
                case WIFI_BW80_BW80:
                case WIFI_BW160: max_rate = 1201800; break;
                default: break;
            }
        }
        else if (protocol_bit_map & WIFI_PROTOCOL_11AC)
        {
            switch (bandwidth)
            {
                case WIFI_BW20: max_rate = 86700; break;
                case WIFI_BW40: max_rate = 200000; break;
                case WIFI_BW80: max_rate = 433300; break;
                case WIFI_BW80_BW80:
                case WIFI_BW160: max_rate = 866000; break;
                default: break;
            }
        }
        else if (protocol_bit_map & WIFI_PROTOCOL_11N)
        {
            switch (bandwidth)
            {
                case WIFI_BW20: max_rate = 72200; break;
                case WIFI_BW40: max_rate = 150000; break;
                default: break;
            }
        }
        max_rate *= spatial_streams;
    }
    else if (protocol_bit_map & (WIFI_PROTOCOL_11A | WIFI_PROTOCOL_11G))
    {
        max_rate = 54000;
    }
    else if (protocol_bit_map & WIFI_PROTOCOL_11B)
    {
        max_rate = 11000;
    }
    else if (protocol_bit_map & WIFI_PROTOCOL_LR)
    {
        max_rate = 2000;
    }

    return max_rate;
}

/**
 * @brief The maximum rate is calculated based on the wifi_ap_record_t
 * 
 * @param ap_info ap record
 * @return int data rate(Kbps)
 */
static int compute_max_rate_from_ap_record(wifi_ap_record_t *ap_info)
{
    uint8_t protocol_bit_map = 0;
    wifi_bandwidth_t bandwidth = ap_info->bandwidth;

    protocol_bit_map |= (ap_info->phy_11b) ? WIFI_PROTOCOL_11B : 0;
    protocol_bit_map |= (ap_info->phy_11g) ? WIFI_PROTOCOL_11G : 0;
    protocol_bit_map |= (ap_info->phy_11n) ? WIFI_PROTOCOL_11N : 0;
    protocol_bit_map |= (ap_info->phy_lr) ? WIFI_PROTOCOL_LR : 0;
    protocol_bit_map |= (ap_info->phy_11a) ? WIFI_PROTOCOL_11A : 0;
    protocol_bit_map |= (ap_info->phy_11ac) ? WIFI_PROTOCOL_11AC : 0;
    protocol_bit_map |= (ap_info->phy_11ax) ? WIFI_PROTOCOL_11AX : 0;

    return compute_max_rate(protocol_bit_map, bandwidth);
}

static void send_ap_record_to_wlan (void *parameter)
{
    wifi_event_sta_scan_done_t *info = (wifi_event_sta_scan_done_t *)parameter;
    wifi_ap_record_t *ap_info = rt_calloc(info->number, sizeof(wifi_ap_record_t));
    uint16_t number = info->number;

    if (esp_wifi_scan_get_ap_records(&number, ap_info) == ESP_OK)
    {
        LOG_D("number of available APs is %d", number);
        for (uint16_t i = 0; i < number; i ++)
        {
            struct rt_wlan_info wlan_info;
            struct rt_wlan_buff buff;

            rt_memset(&wlan_info, 0, sizeof(wlan_info));

            memcpy(wlan_info.bssid, ap_info[i].bssid, sizeof(wlan_info.bssid));
            wlan_info.ssid.len = min(strlen((const char *)ap_info[i].ssid), sizeof(wlan_info.ssid.val));
            rt_memcpy(wlan_info.ssid.val, ap_info[i].ssid, wlan_info.ssid.len);

            if (wlan_info.ssid.len)
                wlan_info.hidden = 0;
            else
                wlan_info.hidden = 1;

            wlan_info.channel = (rt_int16_t)ap_info[i].primary;
            wlan_info.rssi = ap_info[i].rssi;

            wlan_info.datarate = compute_max_rate_from_ap_record(&ap_info[i]) * 1000;
            wlan_info.band = RT_802_11_BAND_2_4GHZ;

            wlan_info.security = security_esp_to_wlan(ap_info[i].authmode);

            buff.data = &wlan_info;
            buff.len = sizeof(wlan_info);

            rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_SCAN_REPORT, &buff);

            LOG_D("bitmask: 11b:%u g:%u n:%u ax: %u lr:%u wps:%u ftm_resp:%u ftm_ini:%u res: %u",
                ap_info[i].phy_11b, ap_info[i].phy_11g,
                ap_info[i].phy_11n, ap_info[i].phy_11ax, ap_info[i].phy_lr,
                ap_info[i].wps, ap_info[i].ftm_responder,
                ap_info[i].ftm_initiator, ap_info[i].reserved
                );
        }

        rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_SCAN_DONE, RT_NULL);
    }
    else
    {
        LOG_E("get ap record failed!");
    }
    rt_free(ap_info);
    rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_SCAN_DONE, RT_NULL);
}

int hosted_wifi_event_post(int32_t event_id, void* event_data, size_t event_data_size, uint32_t ticks_to_wait)
{
    LOG_D("wifi event: %d", event_id);

    switch (event_id)
    {
        case WIFI_EVENT_STA_DISCONNECTED:
        {
            wifi_event_sta_disconnected_t *info = (wifi_event_sta_disconnected_t *)event_data;
            if (info != NULL)
            {
                LOG_W("wifi disconnected, reason: %u", info->reason);
                if (info->reason >= WIFI_REASON_BEACON_TIMEOUT)
                {
                    rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_CONNECT_FAIL, RT_NULL);
                }
                rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_DISCONNECT, RT_NULL);
            }
            else
            {
                LOG_E("sta disconnected info is null");
            }
            break;
        }

        case WIFI_EVENT_STA_CONNECTED:
        {
            wifi_event_sta_connected_t *info = (wifi_event_sta_connected_t *)event_data;
            if (info != NULL)
            {
                LOG_D("wifi connected, ssid: %s, bssid: %02x:%02x:%02x:%02x:%02x:%02x, channel: %d",
                    info->ssid, info->bssid[0], info->bssid[1], info->bssid[2], info->bssid[3], info->bssid[4], info->bssid[5], info->channel);
                rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_CONNECT, RT_NULL);
            }
            else
            {
                LOG_E("sta connected info is null");
            }
            break;
        }

        case WIFI_EVENT_AP_START:
        {
            rt_wlan_dev_indicate_event_handle(wifi_ap.wlan, RT_WLAN_DEV_EVT_AP_START, 0);
            break;
        }

        case WIFI_EVENT_AP_STOP:
        {
            rt_wlan_dev_indicate_event_handle(wifi_ap.wlan, RT_WLAN_DEV_EVT_AP_STOP, 0);
            break;
        }

        case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t *info = (wifi_event_ap_staconnected_t *)event_data;
            struct rt_wlan_info wlan_info;
            struct rt_wlan_buff buff = { .data = &wlan_info, .len = sizeof(struct rt_wlan_info) };
            memcpy(wlan_info.bssid, info->mac, RT_WLAN_BSSID_MAX_LENGTH);
            rt_wlan_dev_indicate_event_handle(wifi_ap.wlan, RT_WLAN_DEV_EVT_AP_ASSOCIATED, &buff);
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            wifi_event_ap_stadisconnected_t *info = (wifi_event_ap_stadisconnected_t *)event_data;
            struct rt_wlan_info wlan_info;
            struct rt_wlan_buff buff = { .data = &wlan_info, .len = sizeof(struct rt_wlan_info) };
            memcpy(wlan_info.bssid, info->mac, RT_WLAN_BSSID_MAX_LENGTH);
            rt_wlan_dev_indicate_event_handle(wifi_ap.wlan, RT_WLAN_DEV_EVT_AP_DISASSOCIATED, &buff);
            break;
        }

        case WIFI_EVENT_SCAN_DONE:
        {
            wifi_event_sta_scan_done_t *info = (wifi_event_sta_scan_done_t *)event_data;
            if (info != NULL && info->number > 0)
            {
                /* create a thread to send scan result to wlan device */
                rt_thread_startup(rt_thread_create("esp_wlan_scan", send_ap_record_to_wlan, info,
                                    ESP_HOSTED_RPC_THREAD_STACK_SIZE, ESP_HOSTED_RPC_THREAD_PRIORITY, 20));
            }
            else
            {
                rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_SCAN_DONE, RT_NULL);
            }
            break;
        }

        default:
        {
            LOG_D("unknown wifi event: %d", event_id);
            break;
        }
    }

    return ESP_OK;
}

static rt_err_t drv_wlan_init(struct rt_wlan_device *wlan)
{
    return RT_EOK;
}

static rt_err_t drv_wlan_mode(struct rt_wlan_device *wlan, rt_wlan_mode_t mode)
{
    return RT_EOK;
}

static rt_err_t drv_wlan_scan(struct rt_wlan_device *wlan, struct rt_scan_info *scan_info)
{
    esp_err_t ret;
    wifi_mode_t mode;

    if (esp_wifi_get_mode(&mode) != ESP_OK)
    {
        LOG_E("get wifi mode failed");
        rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_SCAN_DONE, RT_NULL);
        return -RT_ERROR;
    }
    if (mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA)
    {
        if (mode == WIFI_MODE_AP)
        {
            LOG_D("wifi mode set to APSTA");
            if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK)
            {
                LOG_E("set wifi mode failed");
                rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_SCAN_DONE, RT_NULL);
                return -RT_ERROR;
            }
        }
        else
        {
            LOG_D("wifi mode set to STA");
            if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
            {
                LOG_E("set wifi mode failed");
                rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_SCAN_DONE, RT_NULL);
                return -RT_ERROR;
            }
        }
    }

    if (scan_info != NULL)
    {
        wifi_scan_config_t config =
        {
            .ssid = (uint8_t *)scan_info->ssid.val,
            .show_hidden = false,
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time.active.min = 0,
            .scan_time.active.max = 120,
            .scan_time.passive = 360,
            .home_chan_dwell_time = 30
        };

        LOG_D("scanning for %.*s", scan_info->ssid.len, scan_info->ssid.val);
        ret = esp_wifi_scan_start(&config, false);
    }
    else
    {
        /* configuration settings for scanning, if set to NULL default settings will be used of
         * which default values are show_hidden:false, scan_type:active, scan_time.active.min:0,
         * scan_time.active.max:120 milliseconds, scan_time.passive:360 milliseconds home_chan_dwell_time:30ms
        */
        ret = esp_wifi_scan_start(NULL, false);
    }

    if (ret != ESP_OK)
    {
        LOG_E("scan start failed!");
        rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_SCAN_DONE, RT_NULL);
    }

    return RT_EOK;
}

static rt_err_t drv_wlan_scan_stop(struct rt_wlan_device *wlan)
{
    esp_wifi_scan_stop();
    return RT_EOK;
}

static rt_err_t drv_wlan_join(struct rt_wlan_device *wlan, struct rt_sta_info *sta_info)
{
    wifi_mode_t mode;
    wifi_config_t wifi_config = { 0 };

    if (esp_wifi_get_mode(&mode) != ESP_OK)
    {
        LOG_E("get wifi mode failed");
        rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_CONNECT_FAIL, RT_NULL);
        return -RT_ERROR;
    }
    if (mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA)
    {
        if (mode == WIFI_MODE_AP)
        {
            if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK)
            {
                LOG_E("set wifi mode failed");
                rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_CONNECT_FAIL, RT_NULL);
                return -RT_ERROR;
            }
        }
        else
        {
            if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
            {
                LOG_E("set wifi mode failed");
                rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_CONNECT_FAIL, RT_NULL);
                return -RT_ERROR;
            }
        }
    }

    strncpy((char *) wifi_config.sta.ssid, (const char *)sta_info->ssid.val, sizeof(wifi_config.sta.ssid));
    if (sta_info->key.len)
    {
        strncpy((char *) wifi_config.sta.password, (const char *)sta_info->key.val, sizeof(wifi_config.sta.password));
    }

    if (esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK)
    {
        LOG_E("set sta config failed");
        rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_CONNECT_FAIL, RT_NULL);
        return -RT_ERROR;
    }

    if (esp_wifi_connect() != ESP_OK)
    {
        LOG_E("wifi connect failed");
        rt_wlan_dev_indicate_event_handle(wifi_sta.wlan, RT_WLAN_DEV_EVT_CONNECT_FAIL, RT_NULL);
        return -RT_ERROR;
    }

    return 0;
}

static rt_err_t drv_wlan_disconnect(struct rt_wlan_device *wlan)
{
    esp_wifi_disconnect();
    return 0;
}

static rt_err_t drv_wlan_ap_stop(struct rt_wlan_device *wlan)
{
    wifi_mode_t mode;

    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_APSTA)
    {
        if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
        {
            LOG_E("set wifi mode failed");
            return -RT_ERROR;
        }
    }
    else if (mode == WIFI_MODE_AP)
    {
        if ( esp_wifi_set_mode(WIFI_MODE_NULL) != ESP_OK)
        {
            LOG_E("set wifi mode failed");
            return -RT_ERROR;
        }
    }

    return RT_EOK;
}

static rt_err_t drv_wlan_ap_deauth(struct rt_wlan_device *wlan, rt_uint8_t mac[])
{
    uint16_t aid = 0;

    if (esp_wifi_ap_get_sta_aid(mac, &aid) != ESP_OK)
    {
        LOG_E("get sta aid failed");
        return -RT_ERROR;
    }

    if (esp_wifi_deauth_sta(aid) != ESP_OK)
    {
        LOG_E("deauth sta failed");
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_err_t drv_wlan_softap(struct rt_wlan_device *wlan, struct rt_ap_info *ap_info)
{
    wifi_mode_t mode;

    if (esp_wifi_get_mode(&mode) != ESP_OK)
    {
        LOG_E("get wifi mode failed");
        return -RT_ERROR;
    }

    if (mode != WIFI_MODE_AP && mode != WIFI_MODE_APSTA)
    {
        if (mode == WIFI_MODE_STA)
        {
            if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK)
            {
                LOG_E("set wifi mode failed");
                return -RT_ERROR;
            }
        }
        else
        {
            if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK)
            {
                LOG_E("set wifi mode failed");
                return -RT_ERROR;
            }
        }
    }

    wifi_config_t wifi_config =
    {
        .ap =
        {
            .ssid_hidden = ap_info->hidden,
            .authmode = security_wlan_to_esp(ap_info->security),
            .channel = ap_info->channel,
            .max_connection = ESP_WIFI_MAX_CONN_NUM
        }
    };

    strncpy((char *) wifi_config.ap.ssid, (const char *)ap_info->ssid.val, min(sizeof(wifi_config.ap.ssid), sizeof(ap_info->ssid.val)));
    strncpy((char *) wifi_config.ap.password, (const char *)ap_info->key.val, min(sizeof(wifi_config.ap.password), sizeof(ap_info->key.val)));

    if (esp_wifi_set_config(WIFI_IF_AP, &wifi_config) != ESP_OK)
    {
        LOG_E("set ap config failed");
        return -RT_ERROR;
    }

    return RT_EOK;
}

static int drv_wlan_get_rssi(struct rt_wlan_device *wlan)
{
    int rssi = -127;
    esp_wifi_sta_get_rssi(&rssi);
    return rssi;
}

#ifdef RT_WLAN_DEV_VERSION
static int drv_wlan_get_info(struct rt_wlan_device *wlan, struct rt_wlan_info *info)
{
    wifi_mode_t mode;
    wifi_ap_record_t ap_info;

    if (esp_wifi_get_mode(&mode) != ESP_OK)
    {
        LOG_E("get wifi mode failed");
        return -RT_ERROR;
    }

    if (mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA)
    {
        LOG_E("not in station mode, current %d mode", mode);
        return -RT_ERROR;
    }

    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK)
    {
        LOG_E("not connected to any ap");
        return -RT_ERROR;
    }

    memcpy(info->bssid, ap_info.bssid, sizeof(info->bssid));
    info->ssid.len = min(strlen((const char *)ap_info.ssid), sizeof(info->ssid.val));
    rt_memcpy(info->ssid.val, ap_info.ssid, info->ssid.len);

    info->channel = (rt_int16_t)ap_info.primary;
    info->rssi = ap_info.rssi;

    info->datarate = compute_max_rate_from_ap_record(&ap_info) * 1000;
    info->band = RT_802_11_BAND_2_4GHZ;

    info->security = security_esp_to_wlan(ap_info.authmode);

    return RT_EOK;
}

#if RT_WLAN_DEV_VERSION >= 0x10001 /* >= 1.0.0 */
static int dev_wlan_ap_get_info(struct rt_wlan_device *wlan, struct rt_wlan_info *info)
{
    uint8_t protocol;
    wifi_bandwidth_t bandwidth;
    wifi_config_t config;

    if (esp_wifi_get_config(WIFI_IF_AP, &config) != ESP_OK)
    {
        LOG_E("get ap config failed");
        return -RT_ERROR;
    }

    if (esp_wifi_get_mac(WIFI_IF_AP, info->bssid) != ESP_OK)
    {
        LOG_E("get ap mac failed");
        return -RT_ERROR;
    }

    if (esp_wifi_get_protocol(WIFI_IF_AP, &protocol) != ESP_OK)
    {
        LOG_E("get ap protocol failed");
        return -RT_ERROR;
    }
    if (esp_wifi_get_bandwidth(WIFI_IF_AP, &bandwidth)!= ESP_OK)
    {
        LOG_E("get ap bandwidth failed");
        return -RT_ERROR;
    }

    info->datarate = compute_max_rate(protocol, bandwidth) * 1000;
    info->band = RT_802_11_BAND_2_4GHZ;
    info->channel = config.ap.channel;
    info->hidden = config.ap.ssid_hidden;
    info->security = security_esp_to_wlan(config.ap.authmode);

    return RT_EOK;
}
#endif /* RT_WLAN_DEV_VERSION >= 0x10001 */
#endif /* RT_WLAN_DEV_VERSION */

static rt_err_t drv_wlan_set_powersave(struct rt_wlan_device *wlan, int level)
{
    if (level > WIFI_PS_MAX_MODEM)
    {
        LOG_E("invalid powersave level: %d, plase see wifi_ps_type_t", level);
        return -RT_ERROR;
    }

    return esp_wifi_set_ps((wifi_ps_type_t)level);
}

static int drv_wlan_get_powersave(struct rt_wlan_device *wlan)
{
    wifi_ps_type_t value = WIFI_PS_NONE;

    if (esp_wifi_get_ps(&value) != ESP_OK)
    {
        LOG_E("get power save type failed");
        return -RT_ERROR;
    }

    return value;
}

static rt_err_t drv_wlan_set_channel(struct rt_wlan_device *wlan, int channel)
{
    return esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_ABOVE);
}

static int drv_wlan_get_channel(struct rt_wlan_device *wlan)
{
    uint8_t primary;
    wifi_second_chan_t second;

    if (esp_wifi_get_channel(&primary, &second) != ESP_OK)
    {
        LOG_E("get channel failed");
        return -RT_ERROR;
    }

    return primary;
}

static rt_err_t drv_wlan_set_country(struct rt_wlan_device *wlan, rt_country_code_t cc)
{
    char cc_str[3] = { 0 };

    switch (cc)
    {
        case RT_COUNTRY_CHINA:
            memcpy(cc_str, "CN", 3);
        break;
        default:
            LOG_E("invalid country code: %d", cc);
            return -RT_ERROR;
    }

    if (esp_wifi_set_country_code(cc_str, true) != ESP_OK)
    {
        LOG_E("set country code failed");
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_country_code_t drv_wlan_get_country(struct rt_wlan_device *wlan)
{
    char cc[3] = { 0 };

    if (esp_wifi_get_country_code(cc) != ESP_OK)
    {
        LOG_E("get country code failed");
        return RT_COUNTRY_UNKNOWN;
    }

    LOG_D("country code: %s", cc);

    if (!memcmp(cc, "CN", 3))
    {
        return RT_COUNTRY_CHINA;
    }
    else
    {
        return RT_COUNTRY_UNKNOWN;
    }
}

static rt_err_t drv_wlan_set_mac(struct rt_wlan_device *wlan, rt_uint8_t mac[])
{
    wifi_interface_t ifx = (wlan == wifi_sta.wlan) ? ESP_IF_WIFI_STA : ESP_IF_WIFI_AP;

    return esp_wifi_set_mac(ifx, mac);;
}

static rt_err_t drv_wlan_get_mac(struct rt_wlan_device *wlan, rt_uint8_t mac[])
{
    wifi_interface_t ifx = (wlan == wifi_sta.wlan) ? ESP_IF_WIFI_STA : ESP_IF_WIFI_AP;

    return esp_wifi_get_mac(ifx, mac);
}

static int drv_wlan_send(struct rt_wlan_device *wlan, void *buff, int len)
{
    struct drv_wifi *drv_wifi = get_drv_wifi(wlan);

    if (drv_wifi == &wifi_ap)
    {
        if (esp_wifi_internal_tx(ESP_IF_WIFI_AP, buff, len) != ESP_OK)
        {
            LOG_E("failed to send ap data to wifi drivers");
        }
    }

    if (drv_wifi == &wifi_sta)
    {
        if (esp_wifi_internal_tx(ESP_IF_WIFI_STA, buff, len) != ESP_OK)
        {
            LOG_E("failed to send sta data to wifi drivers");
        }
    }

    return RT_EOK;
}

static int drv_wlan_recv(struct rt_wlan_device *wlan, void *buff, int len)
{
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
    .wlan_ap_deauth = drv_wlan_ap_deauth,
    .wlan_scan_stop = drv_wlan_scan_stop,
    .wlan_get_rssi = drv_wlan_get_rssi,
#ifdef RT_WLAN_DEV_VERSION
    .wlan_get_info = drv_wlan_get_info,
#if RT_WLAN_DEV_VERSION >= 0x10001 /* >= 1.0.1 */
    .wlan_ap_get_info = dev_wlan_ap_get_info,
#endif /* RT_WLAN_DEV_VERSION >= 0x10001 */
#endif /* RT_WLAN_DEV_VERSION */
    .wlan_set_powersave = drv_wlan_set_powersave,
    .wlan_get_powersave = drv_wlan_get_powersave,
    .wlan_cfg_promisc = NULL,
    .wlan_cfg_filter = NULL,
    .wlan_cfg_mgnt_filter = NULL,
    .wlan_set_channel = drv_wlan_set_channel,
    .wlan_get_channel = drv_wlan_get_channel,
    .wlan_set_country = drv_wlan_set_country,
    .wlan_get_country = drv_wlan_get_country,
    .wlan_set_mac = drv_wlan_set_mac,
    .wlan_get_mac = drv_wlan_get_mac,
    .wlan_recv = drv_wlan_recv,
    .wlan_send = drv_wlan_send,
};

static esp_err_t wifi_ap_rx (void *buffer, uint16_t len, void *eb)
{
    if (buffer != NULL)
    {
        rt_wlan_dev_report_data(wifi_ap.wlan, buffer, len);
        esp_wifi_internal_free_rx_buffer(buffer);
    }
    return ESP_OK;
}

static esp_err_t wifi_sta_rx (void *buffer, uint16_t len, void *eb)
{
    if (buffer != NULL)
    {
        rt_wlan_dev_report_data(wifi_sta.wlan, buffer, len);
        esp_wifi_internal_free_rx_buffer(buffer);
    }
    return ESP_OK;
}

static void rt_hw_esp_wlan_init (void *parameter)
{
    static struct rt_wlan_device wlan_ap, wlan_sta;

    /* initialize the esp-hosted */
    if (esp_hosted_init() != ESP_OK)
    {
        LOG_W("esp-hosted init failed!");
        return ;
    }

    /* initialize the wifi driver */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK)
    {
        LOG_E("esp wifi init failed!");
        return ;
    }

#ifndef ESP_HOSTED_RAW_THROUGHPUT_TRANSPORT

    /* set the wifi mode */
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
    {
        LOG_E("esp wifi set mode failed!");
        return ;
    }

    /* set the wifi storage mode */
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK)
    {
        LOG_E("esp wifi set storage failed!");
        return ;
    }

    /* register the wifi driver rx callback */
    esp_wifi_internal_reg_rxcb(WIFI_IF_AP, wifi_ap_rx);
    esp_wifi_internal_reg_rxcb(WIFI_IF_STA, wifi_sta_rx);

    if (esp_wifi_start() != ESP_OK)
    {
        LOG_E("esp wifi start failed!");
        return ;
    }

    /* register the wlan device and set its working mode */
    wifi_ap.wlan = &wlan_ap;
    wifi_sta.wlan = &wlan_sta;

    /* register wlan device for ap */
    if (rt_wlan_dev_register(&wlan_ap, RT_WLAN_DEVICE_AP_NAME, &ops, 0, &wifi_ap) != RT_EOK)
    {
        LOG_E("failed to register a wlan_ap device!");
        return ;
    }

    /* register wlan device for sta */
    if (rt_wlan_dev_register(&wlan_sta, RT_WLAN_DEVICE_STA_NAME, &ops, 0, &wifi_sta) != RT_EOK)
    {
        LOG_E("failed to register a wlan_sta device!");
        return ;
    }

    /* set wlan_sta to STATION mode */
    if (rt_wlan_set_mode(RT_WLAN_DEVICE_STA_NAME, RT_WLAN_STATION) != RT_EOK)
    {
        LOG_E("failed to set %s to station mode!", RT_WLAN_DEVICE_STA_NAME);
        return ;
    }

    /* set wlan_ap to AP mode */
    if (rt_wlan_set_mode(RT_WLAN_DEVICE_AP_NAME, RT_WLAN_AP) != RT_EOK)
    {
        LOG_E("failed to set %s to ap mode!", RT_WLAN_DEVICE_AP_NAME);
        return ;
    }
#endif /* ESP_HOSTED_RAW_THROUGHPUT_TRANSPORT */
}

static int rt_hw_wifi_init (void)
{
#ifdef ESP_HOSTED_THREAD_INIT
    /* use thread initialization */
    rt_thread_t init_thread = rt_thread_create("esp_init", rt_hw_esp_wlan_init, NULL,
                                                ESP_HOSTED_INIT_THREAD_STACK_SIZE,
                                                ESP_HOSTED_INIT_THREAD_PRIORITY, 20);
    RT_ASSERT(init_thread != NULL);
    rt_thread_startup(init_thread);
#else
    /* thread initialization is not used */
    rt_hw_esp_wlan_init(NULL);
#endif

    return RT_EOK;
}
INIT_ENV_EXPORT(rt_hw_wifi_init);
