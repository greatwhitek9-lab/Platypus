#include "channel_hop.h"
#include "radio_ble.h"
#include "proto_framer.h"

#include <zephyr/kernel.h>
#include <string.h>

static enum sniff_mode mode = SNIFF_MODE_ADV;
static const uint8_t adv_channels[3] = {37, 38, 39};
static uint8_t adv_idx;

/* Data channel remapping table for a connection (CONNECT_IND's ChM). */
static uint8_t chan_map[37];
static uint8_t num_used_channels;
static uint8_t hop_increment;
static uint8_t last_unmapped_chan;
static uint32_t conn_access_address;
static uint32_t conn_crc_init;

void channel_hop_init(void)
{
    mode = SNIFF_MODE_ADV;
    adv_idx = 0;
}

void channel_hop_start_adv_scan(void)
{
    mode = SNIFF_MODE_ADV;
    adv_idx = 0;
    radio_ble_set_channel(adv_channels[0], BLE_ADV_ACCESS_ADDRESS,
                   BLE_ADV_CRC_INIT);
}

enum sniff_mode channel_hop_get_mode(void)
{
    return mode;
}

/* BLE Core spec Vol 6, Part B, 4.5.8.2: data channel selection algorithm #1. */
static uint8_t next_data_channel(void)
{
    uint8_t unmapped = (last_unmapped_chan + hop_increment) % 37;
    last_unmapped_chan = unmapped;

    if (chan_map[unmapped]) {
        return unmapped;
    }

    /* remap used channel index deterministically, per spec section 4.5.8.2 */
    uint8_t remainder = unmapped % num_used_channels;
    uint8_t idx = 0, count = 0;
    for (idx = 0; idx < 37; idx++) {
        if (chan_map[idx]) {
            if (count == remainder) {
                return idx;
            }
            count++;
        }
    }
    return unmapped; /* unreachable if chan_map is well formed */
}

/* Parse a CONNECT_IND PDU (PDU type 0x5, seen on an advertising channel)
 * and arm connection-following mode from its fields. Offsets below follow
 * Core spec Vol 6, Part B, 2.3.3.1 (LLData: AA, CRCInit, WinSize, WinOffset,
 * Interval, Latency, Timeout, ChM[5], Hop+SCA). */
static void try_parse_connect_ind(const struct ble_capture *pkt)
{
    if (!pkt->crc_ok || pkt->length < 2 + 34) {
        return;
    }
    const uint8_t *pdu = pkt->pdu;
    uint8_t pdu_type = pdu[0] & 0x0F;
    if (pdu_type != 0x5 /* CONNECT_IND */) {
        return;
    }

    const uint8_t *lldata = &pdu[2] + 12; /* skip header, AdvA, InitA */
    conn_access_address = lldata[0] | (lldata[1] << 8) |
                  (lldata[2] << 16) | ((uint32_t)lldata[3] << 24);
    conn_crc_init = lldata[4] | (lldata[5] << 8) | (lldata[6] << 16);

    const uint8_t *chm_bytes = &lldata[12];
    num_used_channels = 0;
    for (int i = 0; i < 37; i++) {
        bool used = (chm_bytes[i / 8] >> (i % 8)) & 0x1;
        chan_map[i] = used;
        if (used) {
            num_used_channels++;
        }
    }
    hop_increment = lldata[17] & 0x1F;
    last_unmapped_chan = 0;

    if (num_used_channels == 0) {
        return; /* malformed / can't follow */
    }

    mode = SNIFF_MODE_FOLLOW;
    uint8_t first = next_data_channel();
    radio_ble_set_channel(first, conn_access_address, conn_crc_init);
}

void channel_hop_on_capture(const struct ble_capture *pkt)
{
    proto_framer_send_capture(pkt);

    if (mode == SNIFF_MODE_ADV) {
        try_parse_connect_ind(pkt);
        if (mode == SNIFF_MODE_ADV) {
            adv_idx = (adv_idx + 1) % 3;
            radio_ble_set_channel(adv_channels[adv_idx],
                           BLE_ADV_ACCESS_ADDRESS,
                           BLE_ADV_CRC_INIT);
        }
        return;
    }

    /* FOLLOW mode: retune for the next connection event. In this first
     * cut we hop on every received packet rather than on a TIMER1-driven
     * connEvent schedule, which works while packets are flowing but will
     * drift/lose lock during silent connection intervals - see
     * docs/SNIFFER-ARCHITECTURE.md "Known limitations". */
    uint8_t next = next_data_channel();
    radio_ble_set_channel(next, conn_access_address, conn_crc_init);
}
