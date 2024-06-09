#include "note_controller.hpp"
#include "FreeRTOS.h"
#include "freertos/task.h"
#include "usb_midi.h"
#include "midi_msg.h"
#include "debug_gpio.cpp"
#include <algorithm>

#define DEBUG_GPIO GPIO_NUM_5

void NoteController::task_monitor_notes(void* param){
    NoteController* nc = static_cast<NoteController*>(param);
    for(;;){
        nc->process_active_notes();
        vTaskDelay(nc->task_delay_);
    }
}

void NoteController::scan_padset(int padset_id){
    xSemaphoreTake(mux_mutex_, 1);
    uint8_t mux_id = 0;
    if(padset_id > 1){
        mux_id = 1;
    }
    int start = padset_id*8;
    int end = padset_id*8 + 8;
    if(padset_id > 1) end += 1;
    for(int i = (0 + start); i < (end); i++){
        mux_ctrl_->get_value(std::move((MuxController::MUX_IDX) mux_id), 15 - i, pad_buffer_[i]);
    }
    xSemaphoreGive(mux_mutex_);
}

// Read 8 pads
void NoteController::on_pad_hit(uint8_t padset_id){
    //ESP_LOGI("NC", "pad hit id %d", padset_id);
    scan_padset(padset_id);
    int start = padset_id*8;
    int end = padset_id*8 + 8;
    if(padset_id > 2) end += 1;
    int highest = 0;

    // If highest value is above max adc value, the reading is invalid, repeat!
    highest = *std::max_element(pad_buffer_.begin() + start, pad_buffer_.begin() + end);
    if(highest > 5000){
        ESP_LOGE("NC", "adc err");
        on_pad_hit(padset_id);
        return;
    }


    for(int i = start; i < end; i++){
        //ESP_LOGI("NC", "Read signal:%d", pad_buffer_[i]);
        if(pad_buffer_[i] > 100){
            scanned_values_[i] = pad_buffer_[i];
            ignored_pads_.push_back(i);
        }
    }

    if(xSemaphoreTake(active_note_mutex_, 5) == pdFALSE) return;
    // ignored_pads_ will be ignored on rescan, but here they are sent
    for(uint8_t& pad_id : ignored_pads_){
        uint8_t note = calc_note_val_(pad_id);
        uint8_t velocity = calc_velocity_(scanned_values_[pad_id]);
        //ESP_LOGI("NC fp", "Send note %d, raw val %d", note, scanned_values_[pad_id]);
        usb_midi_->send(midi::msg::noteOn(channel_, note, velocity));
        auto exists = std::find_if(active_notes_.begin(), active_notes_.end(), [&note](active_note_t& n){
            return n.note == note;
        });

        if(exists != active_notes_.end()){
            exists->time_remaining = note_length_;
        }
        else{
            active_notes_.emplace_back(active_note_t{note, note_length_});
        }
    }

    xSemaphoreGive(active_note_mutex_);
    // gpio_set_level((gpio_num_t) 21, 0);
}

void NoteController::clear_rescan(){
    scanned_values_.fill(0);
    ignored_pads_.clear();
}

void NoteController::on_rescan(uint8_t padset_id){
    //ESP_LOGI("NC", "rescan");

    scan_padset(padset_id);
    uint8_t start = padset_id*8;
    uint8_t end = start + 8;
    if(padset_id > 2) end += 1;

    int highest = *std::max_element(pad_buffer_.begin() + start, pad_buffer_.begin() + end);
    if(highest > 5000){
        ESP_LOGE("NC", "adc err");
        on_rescan(padset_id);
        return;
    }
    std::vector<uint8_t> new_pad_hits;

    for(int i = start; i < end; i++){
        if(std::find(ignored_pads_.begin(), ignored_pads_.end(), i) != ignored_pads_.end()){
            //ESP_LOGI("nc", "skip ignored");
            continue;
        }
        if(pad_buffer_[i] > 250){
            // ESP_LOGI("nc", "rescan add val");
            scanned_values_[i] = pad_buffer_[i];
            new_pad_hits.push_back(i);
        }
    }

    if(xSemaphoreTake(active_note_mutex_, 2) == pdFALSE) return;
    for(uint8_t& pad_id : new_pad_hits){
        uint8_t note = calc_note_val_(pad_id);
        // ESP_LOGI("rescan", "Send note %d", note);

        usb_midi_->send(midi::msg::noteOn(channel_, note, calc_velocity_(scanned_values_[pad_id])));
        auto exists = std::find_if(active_notes_.begin(), active_notes_.end(), [&note](active_note_t& n){
            return n.note == note;
        });

        if(exists != active_notes_.end()){
            exists->time_remaining = note_length_;
        }
        else{
            active_notes_.emplace_back(active_note_t{note, note_length_});
        }
        ignored_pads_.push_back(pad_id);
    }
    xSemaphoreGive(active_note_mutex_);

}

uint8_t NoteController::calc_note_val_(int pad){
    // Start with middle C = 60
    return(60 + pad + (octave_ * 12) + (transpose_));
}

constexpr uint8_t calc_velo(int adc_val){
    // For the future: The coefficient for velocity should be proportional to channel measured
    // Since the signal becomes smaller over time and each measurement takes ~35 us
    float v =  float(adc_val)/4095*127;
    if(v > 127) v = 127;
    return uint8_t(v);
}

uint8_t NoteController::calc_velocity_(int adc_val){
    return calc_velo(adc_val);
}

void IRAM_ATTR NoteController::process_active_notes(){
    if(xSemaphoreTake(active_note_mutex_, 3) == pdFALSE) return;
    gpio_set_level(DEBUG_GPIO, 1);

    active_notes_.erase(
        std::remove_if(
            active_notes_.begin(),
            active_notes_.end(),
            [&](active_note_t &n) {
                n.time_remaining -= task_delay_;
                if(n.time_remaining <= 0) {
                    usb_midi_->send(midi::msg::noteOff(channel_, n.note));
                }
                return n.time_remaining <= 0;
            }
        ),
        active_notes_.end()
    );
    xSemaphoreGive(active_note_mutex_);
    gpio_set_level(DEBUG_GPIO, 0);
}

NoteController::NoteController(
    MuxController* mux_ctrl,
    usb::UsbMidi* usb_midi,
    uint8_t channel
) : mux_ctrl_(mux_ctrl), usb_midi_(usb_midi), channel_(channel){

    Helpers::setup_debug_gpio(DEBUG_GPIO);
    gpio_set_level(DEBUG_GPIO, 0);
    // Reserve space for 10 active notes at a time. This should be more than enough for low-sustain notes
    active_notes_.reserve(10);
    for(int i = 0; i < 25; i++){
        scanned_values_[i] = 0;
    }
    active_note_mutex_ = xSemaphoreCreateMutex();
    mux_mutex_ = xSemaphoreCreateMutex();
    xTaskCreate(&task_monitor_notes, "mnotes", stack_size_, (void*) this, (BaseType_t) 10, NULL);
}