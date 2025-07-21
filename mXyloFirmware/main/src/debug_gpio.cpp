#include "driver/gpio.h"

class Helpers{
public:
    static void setup_debug_gpio(int num){
        gpio_config_t debug_gpio = {
            .pin_bit_mask = (uint64_t)(1 << num),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE
        };

        gpio_config(&debug_gpio);
    }
};
