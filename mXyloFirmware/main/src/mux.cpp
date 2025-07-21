#include "mux.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "FreeRTOS.h"

int Mux::MUX_ID = 0;

esp_err_t conf_gpio(gpio_num_t gpio){
    esp_err_t err;
    err = gpio_reset_pin(gpio);
    if(err != ESP_OK) return err;
    err = gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    return err;
}
Mux::Mux(mux_pin_config* mux_pins){
        init_mux(mux_pins);
    }
void Mux::init_mux(mux_pin_config* mux_pins){
    mux_pins_.S0 = mux_pins->S0;
    mux_pins_.S1 = mux_pins->S1;
    mux_pins_.S2 = mux_pins->S2;
    mux_pins_.S3 = mux_pins->S3;
    ESP_ERROR_CHECK(conf_gpio(mux_pins_.S0));
    ESP_ERROR_CHECK(conf_gpio(mux_pins_.S1));
    ESP_ERROR_CHECK(conf_gpio(mux_pins_.S2));
    ESP_ERROR_CHECK(conf_gpio(mux_pins_.S3));
    mux_id_ = MUX_ID;
    MUX_ID++;
}

// IT TAKES TOO LONG TO USE SEMAPHORE TAKE HERE!!!
void Mux::switch_channel(uint8_t channel){
    gpio_set_level(mux_pins_.S0, channel & 1);
    gpio_set_level(mux_pins_.S1, (channel >> 1) & 1);
    gpio_set_level(mux_pins_.S2, (channel >> 2) & 1);
    gpio_set_level(mux_pins_.S3, (channel >> 3) & 1);
}

MuxController::MuxController(
    AdcController* adc,
    mux_pin_config* mux_p0,
    mux_pin_config* mux_p1,
    mux_pin_config* mux_misc
    ) : adc_(adc){
    controls_[0] = new Mux(mux_p0);
    controls_[1] = new Mux(mux_p1);
    controls_[2] = new Mux(mux_misc);
}

bool MuxController::get_value(MUX_IDX&& mux_id, const uint8_t& channel, int& value){
    // We can't make it work right now. Accept potential errors
    controls_[mux_id]->switch_channel(channel);
    adc_->read_adc(std::move(AdcController::ADC_CHAN(mux_id)), value);
    return true;
}

// Why do we crash when calling this????
void MuxController::get_value(const MUX_IDX& mux_id, const uint8_t& channel, int& value){
    controls_[mux_id]->switch_channel(channel);
    adc_->read_adc(std::move(AdcController::ADC_CHAN(mux_id)), value);
}
