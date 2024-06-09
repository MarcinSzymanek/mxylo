#include "driver/gpio.h"
#include "FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

esp_err_t conf_gpio(gpio_num_t gpio){
        esp_err_t err;
        err = gpio_reset_pin(gpio);
        if(err != ESP_OK) return err;
        err = gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
        return err;
    }

extern "C" void app_main(void){
    conf_gpio(GPIO_NUM_10);

    while(1){
        ESP_LOGI("Hello", "Send gpio");
        gpio_set_level(GPIO_NUM_10, 1);
        vTaskDelay(100);
        gpio_set_level(GPIO_NUM_10, 0);
        vTaskDelay(100);
    }
}