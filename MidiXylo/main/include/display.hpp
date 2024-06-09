#pragma once
#include "open_lcd.hpp"
#include <string>
#include <array>
#include "FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

class Display{
public:
    Display(){
        content_[0] = "M-Xylo v0.21   CH:1 ";
        content_[1] = "Event:              ";
        content_[2] = "OCT:0  TRP:0  PC:0  ";
        content_[3] = "Mode:PLAY   SUST:OFF";
        event_queue = xQueueCreate(4, sizeof(event_t));
    };

    QueueHandle_t event_queue;

    enum DISP_EVENT : uint8_t{
        MODE = 0,
        OCTAVE = 1,
        TRANSPOSE = 2,
        PROGRAM_CHANGE = 3,
        CONTROL_CHANGE = 4,
        CHANNEL_CHANGE = 5,
        SUSTAIN = 6,
        CONTROL_CHANGE_CHANNEL = 7,
        PITCH_BEND = 8,
        MOD = 9
    };


    typedef std::array<int, 2> event_data_t;

    // Constants which help sort which elements on LCD get updated
    typedef struct event_vals_t{
        const uint8_t line;
        const uint8_t first_val_pos;
        const uint8_t max_val_len;
        const std::string evt_name;
    }event_vals_t;

    event_vals_t octave_evt_vals{
        .line = 2,
        .first_val_pos = 4,
        .max_val_len = 2,
        .evt_name = "OCTAVE"
    };

    event_vals_t transpose_evt_vals{
        .line = 2,
        .first_val_pos = 11,
        .max_val_len = 3,
        .evt_name = "TRANSPOSE"
    };

    event_vals_t cc_chan_switch_vals{
        .line = 2,
        .first_val_pos = 4,
        .max_val_len = 1,
        .evt_name = "CC_CHANGE"
    };

    event_vals_t cc_evt_vals{
        .line = 2,
        .first_val_pos = 4,
        .max_val_len = 1,
        .evt_name = "CC"
    };

    event_vals_t pc_evt_vals{
        .line = 2,
        .first_val_pos = 17,
        .max_val_len = 3,
        .evt_name = "PATCH"
    };

    event_vals_t channel_evt_vals{
        .line = 0,
        .first_val_pos = 18,
        .max_val_len = 2,
        .evt_name = "CHANNEL"
    };

    typedef struct event_t{
        DISP_EVENT type;
        event_data_t data;

        event_t(){};
        event_t(DISP_EVENT t, event_data_t d) : type(t), data(d){}
    }event_t;


    void start_display();
    void push_event(DISP_EVENT&& event_t, event_data_t&& data);
    // Event handlers. Must be public because FreeRTOS tasks have to be static
    void handle_mode_event(uint8_t val);
    void handle_cc_event(uint8_t cc_chan, uint8_t cc_val);
    void handle_events(int val, uint8_t dir, event_vals_t event_vals);

    void handle_cc_channel_switch_event(uint8_t cc_idx, uint8_t new_val);

    void handle_sustain_event(uint8_t val);

    static void task_process_events_(void* params);
private:
    OpenLCD lcd_;

    std::array<std::string, 4> content_;

    std::array<std::string, 2> mode_ = {
        "PLAY",
        "PROG"
    };

    const uint8_t mode_pos{5};
    const uint8_t evt_pos{6};
    const uint8_t sust_pos{17};

    SemaphoreHandle_t mutex_;

};