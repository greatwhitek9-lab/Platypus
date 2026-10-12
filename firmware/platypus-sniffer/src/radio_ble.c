/*
 * Raw nRF52840 RADIO driver for promiscuous BLE capture.
 *
 * The nRF52840 radio has a native "Ble_1Mbit" hardware mode: it does the
 * preamble/access-address detection, de-whitening and CRC check for you,
 * the same silicon a normal BLE controller uses - we just don't run a BLE
 * controller/host on top of it. That's what turns this from "a BlueZ HCI
 * adapter" into something that behaves like Ubertooth: every valid PDU on
 * the tuned channel gets handed to us, addressed to us or not.
 */
#include "radio_ble.h"

#include <hal/nrf_radio.h>
#include <hal/nrf_timer.h>
#include <hal/nrf_ppi.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <string.h>

/* RF channel index (0-39) -> physical BLE channel number -> frequency. */
static uint8_t rf_channel_to_ble_channel(uint8_t rf_chan)
{
    if (rf_chan == 37) return 37;
    if (rf_chan == 38) return 38;
    if (rf_chan == 39) return 39;
    /* Data channels 0-36 (RF index) map 1:1 to BLE data channel 0-36. */
    return rf_chan;
}

static uint32_t ble_channel_to_freq_mhz(uint8_t ble_chan)
{
    /* Per BT Core spec, Vol 6, Part B, 1.4.1. */
    if (ble_chan == 37) return 2402;
    if (ble_chan == 38) return 2426;
    if (ble_chan == 39) return 2480;
    if (ble_chan <= 10) return 2404 + (ble_chan * 2);
    return 2428 + ((ble_chan - 11) * 2);
}

/* Double-buffered PDU RX area the radio DMAs into. */
static uint8_t rx_buf[2][1 + BLE_MAX_PDU_LEN] __attribute__((aligned(4)));
static uint8_t rx_idx;

static ble_capture_cb_t user_cb;
static uint64_t timer1_base_us;

int radio_ble_init(ble_capture_cb_t cb)
{
    user_cb = cb;

    /* TIMER1: free-running 1us tick, used purely as a capture clock so
     * every packet gets a timestamp - this is what the host tool uses to
     * reconstruct packet ordering/intervals in the pcap. */
    nrf_timer_mode_set(NRF_TIMER1, NRF_TIMER_MODE_TIMER);
    nrf_timer_bit_width_set(NRF_TIMER1, NRF_TIMER_BIT_WIDTH_32);
    nrf_timer_frequency_set(NRF_TIMER1, NRF_TIMER_FREQ_1MHz);
    nrf_timer_task_trigger(NRF_TIMER1, NRF_TIMER_TASK_CLEAR);
    nrf_timer_task_trigger(NRF_TIMER1, NRF_TIMER_TASK_START);

    /* Radio power-up defaults; BLE mode + whitening/CRC are configured
     * per-channel in radio_ble_set_channel() because whitening & CRC
     * init depend on which channel / connection we're tuned to. */
    nrf_radio_power_set(NRF_RADIO, true);
    nrf_radio_mode_set(NRF_RADIO, NRF_RADIO_MODE_BLE_1MBIT);

    /* Packet layout matches the BLE LL PDU: 1-byte header (S0), 8-bit
     * length field, 0 extra bits, max payload BLE_MAX_PDU_LEN. */
    nrf_radio_packet_conf_t pcnf = {0};
    pcnf.lflen  = 8;
    pcnf.s0len  = 1;
    pcnf.s1len  = 0;
    pcnf.maxlen = BLE_MAX_PDU_LEN;
    pcnf.statlen = 0;
    pcnf.balen  = 3;      /* 3-byte base + 1-byte prefix = 4-byte AA */
    pcnf.big_endian = false;
    pcnf.whiteen = true;
    nrf_radio_packet_configure(NRF_RADIO, &pcnf);

    nrf_radio_crc_configure(NRF_RADIO, NRF_RADIO_CRC_ADDR_SKIPADDR,
                 0x00065B /* BLE CRC polynomial, init set per-channel */);

    IRQ_CONNECT(RADIO_IRQn, 0, radio_ble_isr_entry, NULL, 0);
    irq_enable(RADIO_IRQn);

    return 0;
}

int radio_ble_set_channel(uint8_t channel, uint32_t access_address,
               uint32_t crc_init)
{
    if (channel > 39) {
        return -EINVAL;
    }

    nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_DISABLE);

    uint8_t ble_chan = rf_channel_to_ble_channel(channel);
    uint32_t freq = ble_channel_to_freq_mhz(ble_chan);
    nrf_radio_frequency_set(NRF_RADIO, freq);

    /* Whitening uses the BLE data-whitening init value, which is the
     * channel index with the top bit set, per Core spec Vol 6 Part B 3.2. */
    nrf_radio_datawhiteiv_set(NRF_RADIO, ble_chan | 0x40);

    nrf_radio_base0_set(NRF_RADIO, (access_address << 8));
    nrf_radio_prefix0_set(NRF_RADIO, (access_address >> 24) & 0xFF);
    nrf_radio_rxaddresses_set(NRF_RADIO, NRF_RADIO_RXADDRESS_ADDRESS0);

    nrf_radio_crcinit_set(NRF_RADIO, crc_init);

    nrf_radio_packetptr_set(NRF_RADIO, rx_buf[rx_idx]);

    nrf_radio_shorts_enable(NRF_RADIO,
        NRF_RADIO_SHORT_READY_START_MASK |
        NRF_RADIO_SHORT_END_START_MASK |
        NRF_RADIO_SHORT_ADDRESS_RSSISTART_MASK);

    nrf_radio_int_enable(NRF_RADIO, NRF_RADIO_INT_END_MASK);

    nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_RXEN);

    return 0;
}

void radio_ble_stop(void)
{
    nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_DISABLE);
}

/* RADIO ISR: fires on END (a packet, matching AA, fully received). */
void radio_ble_isr_entry(const void *arg)
{
    ARG_UNUSED(arg);

    if (!nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_END)) {
        return;
    }
    nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_END);

    uint8_t *buf = rx_buf[rx_idx];
    rx_idx ^= 1;
    nrf_radio_packetptr_set(NRF_RADIO, rx_buf[rx_idx]);

    struct ble_capture pkt = {0};
    pkt.crc_ok = nrf_radio_crc_status_check(NRF_RADIO);
    pkt.rssi_dbm = -(int8_t)nrf_radio_rssi_sample_get(NRF_RADIO);
    pkt.timestamp_us = timer1_base_us + nrf_timer_cc_get(NRF_TIMER1, 0);
    pkt.length = buf[1]; /* length byte (after S0/header byte) */
    if (pkt.length > BLE_MAX_PDU_LEN) {
        pkt.length = BLE_MAX_PDU_LEN;
    }
    memcpy(pkt.pdu, buf, pkt.length + 2);

    if (user_cb) {
        user_cb(&pkt);
    }
}
