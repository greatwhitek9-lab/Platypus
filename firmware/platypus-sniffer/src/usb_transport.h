#ifndef PLATYPUS_USB_TRANSPORT_H_
#define PLATYPUS_USB_TRANSPORT_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Thin wrapper over the Zephyr USB CDC-ACM device so the rest of the
 * firmware never touches Zephyr UART APIs directly. */
int usb_transport_init(void);
void usb_transport_write(const uint8_t *data, size_t len);
bool usb_transport_read_cmd_byte(uint8_t *out);

#endif /* PLATYPUS_USB_TRANSPORT_H_ */
