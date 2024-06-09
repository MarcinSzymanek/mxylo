#pragma once

#include <array>

#include "FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "display.hpp"
#include "mux.hpp"
#include "usb_midi.h"

// This class controls monitoring CC values and processing button/switch events
class PeripheralMonitor{
public:
    PeripheralMonitor(MuxController* mux, QueueHandle_t* cc_queue, QueueHandle_t* button_queue, QueueHandle_t* js_queue);
    void start();

    enum MiscMuxChanMap : uint8_t{
        SUSTAIN = 15,
        PROGRAM = 14,   // This will be a switch, but treat as a button in src
        OCTAVE_UP = 13,
        OCTAVE_DOWN = 12,
        TRANSPOSE_UP = 11,
        TRANSPOSE_DOWN = 10,
        PROGRAM_CHANGE_UP = 9,
        PROGRAM_CHANGE_DOWN = 8,
        // JS
        JS_MOD = 7,
        JS_PITCH = 6,
        // Knobs
        NOTE_DUR = 5,
        CC0 = 4,
        CC1 = 3,
        CC2 = 2,
        CC3 = 1,
    };

    typedef struct cc_event_t{
        uint8_t cc_channel;
        uint8_t value;
    } cc_event_t;

    typedef struct button_event_t{
        MiscMuxChanMap type;
        uint8_t value;
    } button_event_t;

    typedef struct js_event_t{
        MiscMuxChanMap type;
        int value;
    } js_event_t;

    void read_buttons();

    // Mode 0 = Play, 1 = Program
    uint8_t mode{0};

    static void task_monitor_cc(void* params);
    static void task_read_buttons(void* params);
    static void task_monitor_js(void* params);
    static void task_monitor_switches(void* params);

    static TaskHandle_t button_task_handle;
    static QueueHandle_t button_interrupt_queue_;

private:
    int val = 0;
    MuxController::MUX_IDX mux_misc_;
    MuxController* mux_;
    QueueHandle_t* cc_event_queue_;
    QueueHandle_t* button_event_queue_;
    QueueHandle_t* js_event_queue_;

    SemaphoreHandle_t mutex_;
    const int JS_PITCH_DEFAULT = 1979;
    const int JS_MOD_DEFAULT = 1990;
    const int JS_EVENT_THRESH = 250;
    /*
    *  CC MONITORING
    */
    const int monitor_cc_stack_size_ = 4096;
    void scan_cc_();
    void scan_js_();
    void scan_switches_();
    // Array of current raw (adc) cc values
    // Compare against these to check if new CC event should be sent
    std::array<int, 4> current_cc_vals_{0};
    // Mapped cc channels. Controls which CC is being sent to.
    std::array<uint8_t, 4> cc_channels_{7, 10, 65, 84};
    std::array<int, 4> cc_buffer_{0};

    bool js_centered[2]{true, true};
    // How much bigger/lower the new value has to be before it triggers an event
    // Essentially CC sensitivity to change - value should be ~90 to avoid swinging up or down
    const int CC_EVENT_THRESH{60};
    int current_note_duration_raw_{0};
    int note_duration_buffer_{0};
    constexpr bool outside_cc_thresh(const int& old_val, const int& new_val){
        return (new_val > (old_val + CC_EVENT_THRESH) || new_val < (old_val - CC_EVENT_THRESH)); 
    }
    constexpr bool outside_js_thresh(const int& old_val, const int& new_val){
        return (new_val > (old_val + JS_EVENT_THRESH) || new_val < (old_val - JS_EVENT_THRESH)); 
    }
    uint8_t button_status_;
};

