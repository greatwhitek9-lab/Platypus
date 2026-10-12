#ifndef PLATYPUS_CHANNEL_HOP_H_
#define PLATYPUS_CHANNEL_HOP_H_

#include <stdint.h>
#include <stdbool.h>
#include "radio_ble.h"

enum sniff_mode {
    SNIFF_MODE_ADV,      /* promiscuous scan of channels 37/38/39 */
    SNIFF_MODE_FOLLOW,   /* locked onto one established connection */
};

/* Advertising access address / CRC init, fixed by the BLE spec. */
#define BLE_ADV_ACCESS_ADDRESS 0x8E89BED6u
#define BLE_ADV_CRC_INIT       0x555555u

/*
 * Drives the sniffer's capture state machine:
 *  - In ADV mode, round-robins channels 37/38/39 looking for any
 *    advertising PDU (ADV_IND, ADV_NONCONN_IND, SCAN_RSP, ...).
 *  - When told to follow a connection (from a parsed CONNECT_IND), it
 *    switches to FOLLOW mode: computes the BLE channel-selection-algorithm
 *    hop sequence from the CONNECT_IND's channel map / hop increment, and
 *    retunes the radio on each connection event using TIMER1 so we stay
 *    locked to the same channel sequence as the two devices we're
 *    eavesdropping on.
 *
 * NOTE: the connection-following hop timing in this firmware is a first
 * cut. Getting the retune timing tight enough to never miss a connection
 * event needs on-hardware calibration (anchor-point drift, clock skew)
 * that can't be verified without a real board in hand - see
 * docs/SNIFFER-ARCHITECTURE.md for what to tighten up first.
 */
void channel_hop_init(void);
void channel_hop_start_adv_scan(void);
void channel_hop_on_capture(const struct ble_capture *pkt);
enum sniff_mode channel_hop_get_mode(void);

#endif /* PLATYPUS_CHANNEL_HOP_H_ */
