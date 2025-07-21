#pragma once

#include <memory>
#include <array>
#include "FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "adc_ctrl.hpp"
#include "display.hpp"
#include "mux.hpp"
#include "note_controller.hpp"
#include "peripheral_monitor.hpp"
#include "usb_midi.h"

#include "esp_timer.h"

class PadStatus{
public:
    PadStatus(){
        mutex_ = xSemaphoreCreateMutex();
    }
    void get_status(uint8_t& buffer){
        while(xSemaphoreTakeFromISR(mutex_, NULL)){

        }
        buffer = pads_;
        xSemaphoreGiveFromISR(mutex_, NULL);
    }

    void set_padset(const uint8_t& padset){
        while(xSemaphoreTakeFromISR(mutex_, NULL)){

        }
        pads_ |= padset;
        xSemaphoreGiveFromISR(mutex_, NULL);
    }

    void reset_padset(const uint8_t& padset){
        while(xSemaphoreTakeFromISR(mutex_, NULL)){

        }
        pads_ &= ~(padset);
        xSemaphoreGiveFromISR(mutex_, NULL);
    }

    void reset_status(){
        while(xSemaphoreTakeFromISR(mutex_, NULL)){

        }
        pads_ = 0;
        xSemaphoreGiveFromISR(mutex_, NULL);
    }

private:
    uint8_t pads_;
    SemaphoreHandle_t mutex_;
};

class MXylo{
public:
    static const uint8_t PADSET_0{1};
    static const uint8_t PADSET_1{2};
    static const uint8_t PADSET_2{4};

    enum class Mode{
        PLAY,
        PROGRAM,
        NUM_MODES
    };

    Mode mode_ = Mode::PLAY;
    void start();
    static MXylo& instance(){
        static MXylo instance_;

        return instance_;
    }
    MXylo(MXylo const&) = delete;
    void operator=(MXylo const&) = delete;

    esp_timer_handle_t timer_pad_hit;
    esp_timer_handle_t timer_rescan;
    // used for software debouncing
    esp_timer_handle_t timer_button_timeout;
    static void timer_cb_pad_hit(void* param);
    static void timer_cb_rescan(void* param);
    static void timer_cb_button_reset(void* param);
    PadStatus pad_status;
    const int debug = 5;

    // How many times to scan pads again, after a pad has been hit
    const uint8_t max_pad_scan_iterations = 15;
    // On interrupt run timer with 300us value (rise time)
    const int64_t initial_timer_delay_us = 300;
    // Afterwards run timer with 2ms (500 Hz frequency of scanning)
    const int64_t subsequent_timer_delay_us = 5000;
    int current_scan_iteration = 0;
private:
    bool button_timeout = false;
    MXylo();

    AdcController adc_ctrl_;
    MuxController* mux_controller_;
    NoteController* note_controller_;
    PeripheralMonitor* periph_monitor_;

    Display display_{};
    usb::UsbMidi usb_midi_{};

    void switch_mode(uint8_t val);
    // Choose which action to perform based on button press
    void button_action_(PeripheralMonitor::MiscMuxChanMap&& button);
    void play_button_action_(PeripheralMonitor::MiscMuxChanMap&& button);
    void program_button_action_(PeripheralMonitor::MiscMuxChanMap&& button);



    static void task_process_buttons_(void* params);
    static void task_process_cc_(void* params);
    static void task_process_js_(void* params);

    static IRAM_ATTR void button_int_handler(void* params);

    static QueueHandle_t cc_event_queue_;
    static QueueHandle_t button_event_queue_;
    static QueueHandle_t js_event_queue_;

    int timer_count = 0;
    int pad_buffer_pos_ = 0;
    int cc_buffer_pos_ = 0;
    int cc_change_threshold_ = 100;

    int transpose_{0};
    int octave_{0};
    uint8_t channel_{0};
    int8_t patch_{0};
    uint8_t sustain_val = 0;
};
