#include "adc_ctrl.hpp"
#include "mux.hpp"
#include "FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

extern "C" void app_main(void){
    AdcController adc_ctrl;
    std::vector<adc_channel_t> channels{
        adc_channel_t(CONFIG_XMIDI_MUX_CHANNEL_0)
    };
    adc_ctrl.init_adc(channels);
    mux_pin_config_t mux_pins = {
        .S0 = gpio_num_t(CONFIG_MUX0_PIN_S0),
        .S1 = gpio_num_t(CONFIG_MUX0_PIN_S1),
        .S2 = gpio_num_t(CONFIG_MUX0_PIN_S2),
        .S3 = gpio_num_t(CONFIG_MUX0_PIN_S3),
    };
    MuxController mux_ctrl(&mux_pins);

    while(1){
        int val = adc_ctrl.read_adc(0);
        ESP_LOGI("Adc test", "Adc val: %d  ", val);
        vTaskDelay(100);
    }

}