#include "adc_ctrl.hpp"
#include "esp_adc/adc_oneshot.h"
#include "FreeRTOS.h"

AdcController::~AdcController(){
     ESP_ERROR_CHECK(adc_oneshot_del_unit(handle_));
}

void AdcController::init_adc(std::vector<adc_channel_t>&& channels){
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &handle_));

    adc_oneshot_chan_cfg_t config;
    config.bitwidth = ADC_BITWIDTH_DEFAULT;
    config.atten = ADC_ATTEN_DB_12;
    for(adc_channel_t chan : channels){
        ESP_ERROR_CHECK(adc_oneshot_config_channel(handle_, chan, &config));
    }
    channels_ = channels;
}

// Reading from the adc in this way takes approximately 38us. For faster reading, we could use continuous_adc instead
void IRAM_ATTR AdcController::read_adc(const ADC_CHAN& channel, int& buffer){
    adc_oneshot_read(handle_, channels_[(uint8_t)channel], &buffer);
}