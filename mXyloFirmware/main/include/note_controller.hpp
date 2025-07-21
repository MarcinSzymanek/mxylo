#pragma once

#include <stdint.h>
#include <vector>
#include "adc_ctrl.hpp"
#include "mux.hpp"
#include "usb_midi.h"
#include "esp_timer.h"
#include "FreeRTOS.h"
#include "freertos/semphr.h"

class NoteController{
public:
    NoteController(
        MuxController* mux,
        usb::UsbMidi* usb_midi,
        uint8_t channel
    );
    // Trigger, re-trigger or turn off a running note on a pad hit
    void on_pad_hit(uint8_t padset_id);
    void on_rescan(uint8_t padset_id);
    void clear_rescan();
    static void task_monitor_notes(void *);

    // Set note length in ticks (10ms)
    void set_note_length(uint8_t length){
        note_length_ = length;
    }

    void set_channel(uint8_t channel){
        channel_ = channel;
    }

    void set_octave(uint8_t octave){
        octave_ = octave;
    }

    void set_transpose(uint8_t transpose){
        transpose_ = transpose;
    }
private:
    uint8_t calc_note_val_(int pad);
    uint8_t calc_velocity_(int adc_val);
    void scan_padset(int padset_id);
    typedef struct active_note_t{
        uint8_t note;
        int16_t time_remaining;

        active_note_t(uint8_t n, uint16_t t) : note(n), time_remaining(t){};
    }active_note_t;

    std::array<int, 25> scanned_values_;
    std::vector<active_note_t> active_notes_;
    std::vector<uint8_t> ignored_pads_;
    void process_active_notes();
    int8_t octave_{0};
    int8_t transpose_{0};

    SemaphoreHandle_t active_note_mutex_;
    SemaphoreHandle_t mux_mutex_;
    std::array<int, 25> pad_buffer_{0};
    const uint8_t task_delay_ = 2;
    const uint8_t task_priority_ = 5;
    const int stack_size_ = 2048;
    MuxController* mux_ctrl_;
    usb::UsbMidi* usb_midi_;
    bool sustain_ = false;
    uint16_t note_length_;

    uint8_t channel_;
};