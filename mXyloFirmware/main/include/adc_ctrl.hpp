#pragma once

#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include <vector>
#include "FreeRTOS.h"
#include "freertos/semphr.h"

class AdcController{
public:
    enum ADC_CHAN : uint8_t{
        ADC_CHAN_P0 = 0,
        ADC_CHAN_P1 = 1,
        ADC_CHAN_MISC = 2
    };

    AdcController(){
        mutex_ = xSemaphoreCreateMutex();
    }
    ~AdcController();
    void init_adc(std::vector<adc_channel_t> &&channels);
    void read_adc(const ADC_CHAN& channel, int& buffer);


private:

    SemaphoreHandle_t mutex_;
    std::vector<adc_channel_t> channels_;
    adc_oneshot_unit_handle_t handle_;
};