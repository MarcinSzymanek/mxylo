#include "esp_log.h"
#include "esp_timer.h"
#include "adc_ctrl.hpp"
#include "mux.hpp"
#include "usb_midi.h"


void sendMidi(usb::UsbMidi* handle){
    ESP_LOGI("MidiXylo", "MidiXylo test");
    handle->send(midi::msg::noteOn(1, 60, 60));
}

static void timer_cb(void* args){
    usb::UsbMidi* usb_handle = static_cast<usb::UsbMidi*>(args);

    sendMidi(usb_handle);
}

extern "C" void app_main(void){
    
    // Test UsbMidi
    usb::UsbMidi usb_handle;

    usb_handle.send(midi::msg::noteOn(0, 60, 60));

    const esp_timer_create_args_t task_args = {
        .callback = &timer_cb,
        .arg = &usb_handle,
        .name = "send midi cb"
    };
    esp_timer_handle_t timer_handle;
    esp_timer_create(&task_args, &timer_handle);

    esp_timer_start_periodic(timer_handle, 1000*1000);
    while(1){
        vTaskDelay(100);
    }

}