#include "proto_framer.h"
#include "usb_transport.h"
#include "channel_hop.h"

#include <string.h>

#define FW_VERSION_STRING "platypus-sniffer-0.1.0"

static uint16_t crc16_ccitt_false(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
        }
    }
    return crc;
}

static void send_frame(uint8_t type, const uint8_t *payload, uint16_t len)
{
    uint8_t header[5];
    header[0] = PROTO_MAGIC0;
    header[1] = PROTO_MAGIC1;
    header[2] = (len + 1) & 0xFF;
    header[3] = ((len + 1) >> 8) & 0xFF;
    header[4] = type;

    uint8_t crc_buf[1 + 256];
    crc_buf[0] = type;
    if (len) {
        memcpy(&crc_buf[1], payload, len);
    }
    uint16_t crc = crc16_ccitt_false(crc_buf, len + 1);
    uint8_t crc_bytes[2] = { crc & 0xFF, (crc >> 8) & 0xFF };

    usb_transport_write(header, sizeof(header));
    if (len) {
        usb_transport_write(payload, len);
    }
    usb_transport_write(crc_bytes, sizeof(crc_bytes));
}

void proto_framer_init(void)
{
    usb_transport_init();
}

void proto_framer_send_capture(const struct ble_capture *pkt)
{
    /* Wire layout kept flat/explicit (no struct padding games) so
     * tools/platypus_proto.py can decode it with a plain struct.unpack. */
    uint8_t buf[4 + 1 + 1 + 1 + 4 + 8 + 1 + BLE_MAX_PDU_LEN];
    size_t off = 0;

    memcpy(&buf[off], &pkt->access_address, 4); off += 4;
    buf[off++] = pkt->channel;
    buf[off++] = (uint8_t)pkt->rssi_dbm;
    buf[off++] = pkt->crc_ok ? 1 : 0;
    memcpy(&buf[off], &pkt->crc, 4); off += 4;
    memcpy(&buf[off], &pkt->timestamp_us, 8); off += 8;
    buf[off++] = pkt->length;
    memcpy(&buf[off], pkt->pdu, pkt->length); off += pkt->length;

    send_frame(PROTO_MSG_CAPTURE, buf, (uint16_t)off);
}

void proto_framer_send_version(void)
{
    send_frame(PROTO_MSG_VERSION, (const uint8_t *)FW_VERSION_STRING,
           sizeof(FW_VERSION_STRING) - 1);
}

void proto_framer_poll_host_commands(void)
{
    uint8_t cmd;
    while (usb_transport_read_cmd_byte(&cmd)) {
        switch (cmd) {
        case PROTO_MSG_PING:
            send_frame(PROTO_MSG_PONG, NULL, 0);
            break;
        case PROTO_MSG_SET_MODE:
            channel_hop_start_adv_scan();
            break;
        default:
            break;
        }
    }
}
