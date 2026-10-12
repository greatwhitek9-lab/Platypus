/*
 * Platypus Sniffer firmware entry point.
 *
 * On boot: bring up USB (framed protocol transport), bring up the raw BLE
 * radio, and start promiscuous scanning of the three advertising channels.
 * From there, channel_hop.c drives everything else (including opportunistic
 * connection-following when it sees a CONNECT_IND).
 */
#include <zephyr/kernel.h>

#include "radio_ble.h"
#include "channel_hop.h"
#include "proto_framer.h"

static void on_capture(const struct ble_capture *pkt)
{
    channel_hop_on_capture(pkt);
}

int main(void)
{
    proto_framer_init();
    proto_framer_send_version();

    radio_ble_init(on_capture);
    channel_hop_init();
    channel_hop_start_adv_scan();

    while (1) {
        proto_framer_poll_host_commands();
        k_sleep(K_MSEC(5));
    }

    return 0;
}
