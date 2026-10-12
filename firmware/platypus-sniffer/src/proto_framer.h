#ifndef PLATYPUS_PROTO_FRAMER_H_
#define PLATYPUS_PROTO_FRAMER_H_

#include "radio_ble.h"

/*
 * Wire protocol between the dongle and tools/platypus_proto.py on the host.
 * Deliberately simple/textbook so the host side is easy to read & extend -
 * this is *not* trying to be HCI; it's a thin framed pipe for raw capture
 * records and a handful of control commands.
 *
 * Frame: 0xAA 0x55 | len:u16 LE | type:u8 | payload[len] | crc16:u16 LE
 * CRC16 is CRC-16/CCITT-FALSE over (len bytes of type+payload).
 */
#define PROTO_MAGIC0 0xAA
#define PROTO_MAGIC1 0x55

enum proto_msg_type {
    PROTO_MSG_CAPTURE   = 0x01, /* device -> host: a struct ble_capture */
    PROTO_MSG_PING      = 0x02, /* host -> device */
    PROTO_MSG_PONG      = 0x03, /* device -> host */
    PROTO_MSG_SET_MODE  = 0x04, /* host -> device: 0=adv scan,1=idle */
    PROTO_MSG_VERSION   = 0x05, /* device -> host: version string */
};

void proto_framer_init(void);
void proto_framer_send_capture(const struct ble_capture *pkt);
void proto_framer_send_version(void);
void proto_framer_poll_host_commands(void);

#endif /* PLATYPUS_PROTO_FRAMER_H_ */
