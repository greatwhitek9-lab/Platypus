#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/addr.h>

#include "passive_survey.h"

#define HEX_PREVIEW_BYTES 24
#define NAME_MAX_LEN      48
#define HEX_MAX_LEN       ((HEX_PREVIEW_BYTES * 2) + 1)

struct parsed_ad {
    char name[NAME_MAX_LEN];
    char mfg_hex[HEX_MAX_LEN];
    char svc16_hex[HEX_MAX_LEN];
    uint8_t flags;
    bool has_flags;
    bool has_tx_power;
    int8_t adv_tx_power;
};

static bool survey_running;
static bool scan_cb_registered;
static uint32_t adv_events;
static uint32_t named_events;
static uint32_t mfg_events;
static uint32_t svc_events;
static int8_t strongest_rssi = -127;
static int8_t weakest_rssi = 127;

static const struct bt_le_scan_param np_scan_param = {
    .type = BT_LE_SCAN_TYPE_PASSIVE,
    .options = BT_LE_SCAN_OPT_NONE,
    .interval = 0x0060,
    .window = 0x0030,
};

static void safe_copy_name(char *dst, size_t dst_len, const uint8_t *src, uint8_t src_len)
{
    size_t n = MIN((size_t)src_len, dst_len - 1);

    for (size_t i = 0; i < n; i++) {
        char c = (char)src[i];

        if (c == '"' || c == '\\') {
            dst[i] = '_';
        } else if (isprint((unsigned char)c)) {
            dst[i] = c;
        } else {
            dst[i] = '.';
        }
    }

    dst[n] = '\0';
}

static void hex_preview(char *dst, size_t dst_len, const uint8_t *src, uint8_t src_len)
{
    static const char hex[] = "0123456789abcdef";
    size_t n = MIN((size_t)src_len, (dst_len - 1) / 2);
    size_t out = 0;

    for (size_t i = 0; i < n; i++) {
        dst[out++] = hex[(src[i] >> 4) & 0x0f];
        dst[out++] = hex[src[i] & 0x0f];
    }

    dst[out] = '\0';
}

static bool parse_ad_cb(struct bt_data *data, void *user_data)
{
    struct parsed_ad *parsed = user_data;

    switch (data->type) {
    case BT_DATA_FLAGS:
        if (data->data_len >= 1) {
            parsed->flags = data->data[0];
            parsed->has_flags = true;
        }
        break;

    case BT_DATA_NAME_SHORTENED:
    case BT_DATA_NAME_COMPLETE:
        if (parsed->name[0] == '\0' || data->type == BT_DATA_NAME_COMPLETE) {
            safe_copy_name(parsed->name, sizeof(parsed->name),
                           data->data, data->data_len);
        }
        break;

    case BT_DATA_TX_POWER:
        if (data->data_len >= 1) {
            parsed->adv_tx_power = (int8_t)data->data[0];
            parsed->has_tx_power = true;
        }
        break;

    case BT_DATA_MANUFACTURER_DATA:
        if (parsed->mfg_hex[0] == '\0') {
            hex_preview(parsed->mfg_hex, sizeof(parsed->mfg_hex),
                        data->data, data->data_len);
        }
        break;

    case BT_DATA_SVC_DATA16:
        if (parsed->svc16_hex[0] == '\0') {
            hex_preview(parsed->svc16_hex, sizeof(parsed->svc16_hex),
                        data->data, data->data_len);
        }
        break;

    default:
        break;
    }

    return true;
}

static const char *phy_to_str(uint8_t phy)
{
    switch (phy) {
    case BT_GAP_LE_PHY_1M:
        return "1M";
    case BT_GAP_LE_PHY_2M:
        return "2M";
    case BT_GAP_LE_PHY_CODED:
        return "CODED";
    default:
        return "unknown";
    }
}

static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
    struct parsed_ad parsed = {0};
    char addr[BT_ADDR_LE_STR_LEN];
    uint8_t ad_len = buf ? buf->len : 0U;

    adv_events++;

    if (info->rssi > strongest_rssi) {
        strongest_rssi = info->rssi;
    }

    if (info->rssi < weakest_rssi) {
        weakest_rssi = info->rssi;
    }

    if (buf != NULL) {
        bt_data_parse(buf, parse_ad_cb, &parsed);
    }

    if (parsed.name[0] != '\0') {
        named_events++;
    }

    if (parsed.mfg_hex[0] != '\0') {
        mfg_events++;
    }

    if (parsed.svc16_hex[0] != '\0') {
        svc_events++;
    }

    bt_addr_le_to_str(info->addr, addr, sizeof(addr));

    printk("{\"type\":\"adv\","
           "\"ts_ms\":%u,"
           "\"addr\":\"%s\","
           "\"rssi\":%d,"
           "\"phy\":\"%s\","
           "\"sid\":%u,"
           "\"interval\":%u,"
           "\"ad_len\":%u",
           k_uptime_get_32(),
           addr,
           info->rssi,
           phy_to_str(info->primary_phy),
           info->sid,
           info->interval,
           ad_len);

    if (parsed.has_flags) {
        printk(",\"flags\":%u", parsed.flags);
    }

    if (parsed.has_tx_power) {
        printk(",\"adv_tx_power\":%d", parsed.adv_tx_power);
    }

    if (parsed.name[0] != '\0') {
        printk(",\"name\":\"%s\"", parsed.name);
    }

    if (parsed.mfg_hex[0] != '\0') {
        printk(",\"mfg_hex\":\"%s\"", parsed.mfg_hex);
    }

    if (parsed.svc16_hex[0] != '\0') {
        printk(",\"svc16_hex\":\"%s\"", parsed.svc16_hex);
    }

    printk("}\n");
}

static struct bt_le_scan_cb np_scan_callbacks = {
    .recv = scan_recv,
};

int np_passive_survey_start(void)
{
    int err;

    if (!scan_cb_registered) {
        bt_le_scan_cb_register(&np_scan_callbacks);
        scan_cb_registered = true;
    }

    if (survey_running) {
        return 0;
    }

    err = bt_le_scan_start(&np_scan_param, NULL);
    if (err == 0) {
        survey_running = true;
        printk("{\"type\":\"status\",\"tool\":\"ble_passive_survey\",\"scan\":\"on\"}\n");
    }

    return err;
}

int np_passive_survey_stop(void)
{
    int err;

    if (!survey_running) {
        return 0;
    }

    err = bt_le_scan_stop();
    if (err == 0) {
        survey_running = false;
        printk("{\"type\":\"status\",\"tool\":\"ble_passive_survey\",\"scan\":\"off\"}\n");
    }

    return err;
}

int np_passive_survey_status(void)
{
    printk("{\"type\":\"survey_status\","
           "\"scanning\":%s,"
           "\"adv_events\":%u,"
           "\"named_events\":%u,"
           "\"mfg_events\":%u,"
           "\"svc_events\":%u,"
           "\"strongest_rssi\":%d,"
           "\"weakest_rssi\":%d}\n",
           survey_running ? "true" : "false",
           adv_events,
           named_events,
           mfg_events,
           svc_events,
           strongest_rssi,
           weakest_rssi);

    return 0;
}

int np_passive_survey_reset(void)
{
    adv_events = 0;
    named_events = 0;
    mfg_events = 0;
    svc_events = 0;
    strongest_rssi = -127;
    weakest_rssi = 127;

    printk("{\"type\":\"status\",\"tool\":\"ble_passive_survey\",\"stats\":\"reset\"}\n");
    return 0;
}

bool np_passive_survey_is_running(void)
{
    return survey_running;
}
