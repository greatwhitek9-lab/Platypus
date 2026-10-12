/*
 * USB CDC-ACM transport, built against Zephyr's newer "USB device stack
 * next" (USBD) API rather than the older, now-deprecated USB device stack.
 *
 * This board's devicetree declares a single CDC-ACM UART node
 * (compatible = "zephyr,cdc-acm-uart"), and BOTH the legacy stack's class
 * driver and this newer stack's class driver bind to that same node by
 * default - trying to enable the legacy stack alongside this one fails the
 * link with "multiple definition of ...dts_ord...". So the legacy stack is
 * kept off (CONFIG_USB_DEVICE_STACK=n) and everything here goes through the
 * USBD_* APIs instead, following Zephyr's own samples/subsys/usb/cdc_acm
 * reference implementation.
 *
 * Despite the different init path, the actual byte I/O below
 * (uart_poll_out / uart_poll_in) is unchanged from a legacy-stack
 * implementation - both stacks' CDC-ACM class driver expose the same
 * standard Zephyr UART driver API for the resulting virtual UART device.
 */
#include "usb_transport.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(platypus_usb, LOG_LEVEL_INF);

static const struct device *cdc_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

USBD_DEVICE_DEFINE(platypus_usbd,
                    DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
                    0x239A, 0x0002);

USBD_DESC_LANG_DEFINE(platypus_lang);
USBD_DESC_MANUFACTURER_DEFINE(platypus_mfr, "greatwhitek9-lab");
USBD_DESC_PRODUCT_DEFINE(platypus_product, "Platypus BLE Sniffer");

USBD_DESC_CONFIG_DEFINE(platypus_fs_cfg_desc, "FS Configuration");

/* Bus-powered, no remote wakeup - matches the bus-powered USB_MAX_POWER
 * budget this firmware targets for phone/tablet OTG power budgeting. */
static const uint8_t platypus_usbd_attributes = 0;

USBD_CONFIGURATION_DEFINE(platypus_fs_config, platypus_usbd_attributes, 96,
                           &platypus_fs_cfg_desc);

static K_SEM_DEFINE(dtr_sem, 0, 1);

static void platypus_fix_code_triple(struct usbd_context *uds_ctx,
                      const enum usbd_speed speed)
{
    if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS)) {
        /* CDC ACM has an Interface Association Descriptor - use the
         * triple that signals that instead of a bare single-interface
         * class code. */
        usbd_device_set_code_triple(uds_ctx, speed, USB_BCC_MISCELLANEOUS,
                         0x02, 0x01);
    } else {
        usbd_device_set_code_triple(uds_ctx, speed, 0, 0, 0);
    }
}

static void platypus_usbd_msg_cb(struct usbd_context *const ctx,
                   const struct usbd_msg *msg)
{
    if (usbd_can_detect_vbus(ctx)) {
        if (msg->type == USBD_MSG_VBUS_READY) {
            if (usbd_enable(ctx)) {
                LOG_ERR("Failed to enable USB device support");
            }
        }
        if (msg->type == USBD_MSG_VBUS_REMOVED) {
            if (usbd_disable(ctx)) {
                LOG_ERR("Failed to disable USB device support");
            }
        }
    }

    if (msg->type == USBD_MSG_CDC_ACM_CONTROL_LINE_STATE) {
        uint32_t dtr = 0U;

        uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_DTR, &dtr);
        if (dtr) {
            k_sem_give(&dtr_sem);
        }
    }
}

static int platypus_usbd_setup(void)
{
    int err;

    err = usbd_add_descriptor(&platypus_usbd, &platypus_lang);
    if (err) {
        LOG_ERR("Failed to add language descriptor (%d)", err);
        return err;
    }
    err = usbd_add_descriptor(&platypus_usbd, &platypus_mfr);
    if (err) {
        LOG_ERR("Failed to add manufacturer descriptor (%d)", err);
        return err;
    }
    err = usbd_add_descriptor(&platypus_usbd, &platypus_product);
    if (err) {
        LOG_ERR("Failed to add product descriptor (%d)", err);
        return err;
    }

    err = usbd_add_configuration(&platypus_usbd, USBD_SPEED_FS,
                      &platypus_fs_config);
    if (err) {
        LOG_ERR("Failed to add Full-Speed configuration (%d)", err);
        return err;
    }

    err = usbd_register_all_classes(&platypus_usbd, USBD_SPEED_FS, 1, NULL);
    if (err) {
        LOG_ERR("Failed to register USB classes (%d)", err);
        return err;
    }

    platypus_fix_code_triple(&platypus_usbd, USBD_SPEED_FS);
    usbd_self_powered(&platypus_usbd,
               platypus_usbd_attributes & USB_SCD_SELF_POWERED);

    err = usbd_msg_register_cb(&platypus_usbd, platypus_usbd_msg_cb);
    if (err) {
        LOG_ERR("Failed to register USBD message callback (%d)", err);
        return err;
    }

    return 0;
}

int usb_transport_init(void)
{
    int err;

    if (!device_is_ready(cdc_dev)) {
        LOG_ERR("CDC ACM device not ready");
        return -ENODEV;
    }

    err = platypus_usbd_setup();
    if (err) {
        return err;
    }

    err = usbd_init(&platypus_usbd);
    if (err) {
        LOG_ERR("Failed to initialize USB device support (%d)", err);
        return err;
    }

    if (!usbd_can_detect_vbus(&platypus_usbd)) {
        err = usbd_enable(&platypus_usbd);
        if (err) {
            LOG_ERR("Failed to enable USB device support (%d)", err);
            return err;
        }
    }

    /* Block until the host (desktop tool, Wireshark extcap, or the
     * Android app) actually opens the serial port, same as the previous
     * legacy-stack implementation did via a DTR poll loop - this version
     * just reacts to the USBD control-line-state message instead of
     * busy-polling. */
    k_sem_take(&dtr_sem, K_FOREVER);

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
