/*
 * Platypus Sniffer - raw nRF52840 RADIO peripheral driven in Bluetooth LE
 * PHY mode, used for promiscuous capture instead of Zephyr's BT controller.
 *
 * This is the same general approach used by well-known open BLE sniffer
 * projects on the nRF52 series (e.g. Sniffle, nRF Sniffer): put the
 * peripheral's hardware BLE mode to work directly so we see every packet
 * on a channel, not just packets addressed to us.
 */
#ifndef PLATYPUS_RADIO_BLE_H_
#define PLATYPUS_RADIO_BLE_H_

#include <stdint.h>
#include <stdbool.h>

#define BLE_MAX_PDU_LEN 255

/* One captured over-the-air packet, as handed up from the radio ISR. */
struct ble_capture {
    uint32_t access_address;
    uint8_t  channel;        /* 0-39 (RF channel index, not freq) */
    int8_t   rssi_dbm;
    uint32_t crc;
    bool     crc_ok;
    uint64_t timestamp_us;   /* since sniffer boot, from TIMER1 */
    uint8_t  length;
    uint8_t  pdu[BLE_MAX_PDU_LEN];
};

typedef void (*ble_capture_cb_t)(const struct ble_capture *pkt);

/* Bring up RADIO + TIMER1 (free-running capture clock) + PPI wiring. */
int radio_ble_init(ble_capture_cb_t cb);

/*
 * Tune to one of the 40 BLE RF channels (0-39) and start listening.
 * access_address / crc_init select what the hardware address-matches and
 * what CRC seed it de-whitens/validates against:
 *   - On advertising channels (37/38/39) always use the advertising
 *     access address 0x8E89BED6 and CRC init 0x555555.
 *   - When following a connection, pass the CONNECT_IND's AA and the
 *     3-byte CRCInit from the same PDU (see channel_hop.c).
 */
int radio_ble_set_channel(uint8_t channel, uint32_t access_address,
               uint32_t crc_init);

/* Stop receiving (radio to DISABLED state). */
void radio_ble_stop(void);

#endif /* PLATYPUS_RADIO_BLE_H_ */
