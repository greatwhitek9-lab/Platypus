#include "usb_transport.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

static const struct device *cdc_dev;

int usb_transport_init(void)
{
    cdc_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
    if (!device_is_ready(cdc_dev)) {
        return -ENODEV;
    }

    int ret = usb_enable(NULL);
    if (ret != 0 && ret != -EALREADY) {
        return ret;
    }

    uint32_t dtr = 0;
    while (!dtr) {
        uart_line_ctrl_get(cdc_dev, UART_LINE_CTRL_DTR, &dtr);
        k_sleep(K_MSEC(50));
    }

    return 0;
}

void usb_transport_write(const uint8_t *data, size_t len)
{
    if (!cdc_dev) {
        return;
    }
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(cdc_dev, data[i]);
    }
}

bool usb_transport_read_cmd_byte(uint8_t *out)
{
    if (!cdc_dev) {
        return false;
    }
    return uart_poll_in(cdc_dev, out) == 0;
}
