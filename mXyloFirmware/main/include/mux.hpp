#pragma once

#include "stdint.h"
#include "array"
#include "esp_log.h"
#include "driver/gpio.h"
#include "FreeRTOS.h"
#include "freertos/semphr.h"

#include "adc_ctrl.hpp"

typedef struct mux_pin_config_t{
    gpio_num_t S0;
    gpio_num_t S1;
    gpio_num_t S2;
    gpio_num_t S3;
} mux_pin_config;

class Mux{
public:
    Mux(){}
    Mux(mux_pin_config* mux_pins);
    void init_mux(mux_pin_config* mux_pins);
    void switch_channel(uint8_t channel);
private:
    mux_pin_config mux_pins_;
    static int MUX_ID;
    int mux_id_;
};

class MuxController{
public:
    enum MUX_IDX : uint8_t{
        MUX_P0 = 0,
        MUX_P1 = 1,
        MUX_MISC = 2,
    };
    MuxController(
        AdcController* adc,
        mux_pin_config* mux_p0,
        mux_pin_config* mux_p1,
        mux_pin_config* mux_misc
    );

    // Read an adc value from a specific mux on a specific channel, store it in a buffer
    bool get_value(MUX_IDX&& mux_id, const uint8_t& channel, int& value);
    // Same as above but... crashes???
    void get_value(const MUX_IDX& mux_id, const uint8_t& channel, int& value);

    // We have only one adc - we have to control who gets it when
    // Ideally this class would be a singleton
    // This is NOT an ideal solution, as it can block reading a pad hit

    bool take(){
        if(xSemaphoreTake(mutex_, 1) == pdTRUE){
            return true;
        }
        return false;
    }

    void release(){
        xSemaphoreGive(mutex_);
    }


private:
    SemaphoreHandle_t mutex_;
    AdcController* adc_;
    std::array<Mux*, 3> controls_;
};


