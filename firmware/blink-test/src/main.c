#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/*
 * Heltec T114 v2 green LED:
 * Zephyr board docs show LED4 / green LED = P1.03
 */
#define LED_PORT_NODE DT_NODELABEL(gpio1)
#define LED_PIN 3

static const struct device *const led_port = DEVICE_DT_GET(LED_PORT_NODE);

int main(void)
{
    if (!device_is_ready(led_port)) {
        while (1) {
            k_sleep(K_SECONDS(1));
        }
    }

    gpio_pin_configure(led_port, LED_PIN, GPIO_OUTPUT_INACTIVE);

    while (1) {
        gpio_pin_toggle(led_port, LED_PIN);
        k_sleep(K_MSEC(500));
    }

    return 0;
}
