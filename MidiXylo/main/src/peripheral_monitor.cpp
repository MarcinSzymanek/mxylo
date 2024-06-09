#include "peripheral_monitor.hpp"
#include "cmath"

// calculate midi value from raw adc
constexpr uint8_t calc_raw_to_midi(int adc_val){
    float v = float(adc_val)/4095*127;
    return uint8_t(v);
}

// calculate midi value from raw adc
constexpr uint8_t calc_raw_to_js(int adc_val){
    float v = float(adc_val - 1980)/2115*127;
    if(v < 0) v = 0;
    if(v > 127) v = 127;
    return uint8_t(v);
}

constexpr int16_t calc_raw_to_pitch(int adc_val){
    float v = float(adc_val) * 4.000;
    return(int(floor(v)));
}

void PeripheralMonitor::task_monitor_cc(void* params){
    PeripheralMonitor* pmon = static_cast<PeripheralMonitor*>(params);
    int delay;
    for(;;){
        pmon->scan_cc_();
        if(pmon->mode) delay = 15;
        else delay = 2;
        vTaskDelay(delay);
    }
}

TaskHandle_t PeripheralMonitor::button_task_handle;
QueueHandle_t PeripheralMonitor::button_interrupt_queue_;

void PeripheralMonitor::task_monitor_js(void* params){
    PeripheralMonitor* pmon = static_cast<PeripheralMonitor*>(params);
    for(;;){
        pmon->scan_js_();
        vTaskDelay(3);
    }
}

void PeripheralMonitor::scan_js_(){
    // Mutex used to exclusively use mux_
    if(xSemaphoreTake(mutex_, 1) == pdFALSE) return;

    int js_buffer[2];
    mux_->get_value(
            std::move(MuxController::MUX_IDX::MUX_MISC),
            uint8_t(MiscMuxChanMap::JS_MOD),
            js_buffer[0]
        );
    mux_->get_value(
            std::move(MuxController::MUX_IDX::MUX_MISC),
            uint8_t(MiscMuxChanMap::JS_PITCH),
            js_buffer[1]
        );
    xSemaphoreGive(mutex_);
    //ESP_LOGI("js", "js monitor event vals x %d, y: %d", js_buffer[0], js_buffer[1]);
    js_event_t event;
    event.type = MiscMuxChanMap::JS_MOD;

    // Here we have to keep track on whether or not joystick is centered
    // If it's not but the value is within threshold - center it
    // This is necessary, so that joystick values return to default
    // When joystick is in default position (centered)
    if(js_buffer[0] > (JS_MOD_DEFAULT + JS_EVENT_THRESH)){
        // send mod event
        event.value = calc_raw_to_js(js_buffer[0]);
        xQueueSend(*js_event_queue_, &event, 1);
        js_centered[0] = false;
    }
    else if(!js_centered[0]){
        event.value = 0;
        xQueueSend(*js_event_queue_, &event, 5);
        js_centered[0] = true;
    }
    event.type = MiscMuxChanMap::JS_PITCH;
    if(outside_js_thresh(JS_PITCH_DEFAULT, js_buffer[1])){
        // send pitch event
        event.value = calc_raw_to_pitch(js_buffer[1]);
        js_centered[1] = false;
        xQueueSend(*js_event_queue_, &event, 1);
    }
    else if(!js_centered[1]){
        event.value = 0x2000;
        xQueueSend(*js_event_queue_, &event, 5);
        js_centered[1] = true;
    }
}

// Play/Program switch is the least important, so it gets a large delay
// between polling
void PeripheralMonitor::task_monitor_switches(void* params){
    PeripheralMonitor* pmon = static_cast<PeripheralMonitor*>(params);
    // Read switches every 250ms
    for(;;){
        pmon->scan_switches_();
        vTaskDelay(25);
    }
}

void PeripheralMonitor::scan_switches_(){
    int button_buffer;
    // Mutex used to exclusively used mux_ and button_event_queue_
    if(xSemaphoreTake(mutex_, 5) == pdFALSE) return;
    // Just check program
    mux_->get_value(std::move(MuxController::MUX_IDX::MUX_MISC), uint8_t(MiscMuxChanMap::PROGRAM), button_buffer);
    if(button_buffer > 3000 && mode == 0){
        button_event_t event{
        .type = MiscMuxChanMap::PROGRAM,
        .value = 1
        };
        xQueueSend(*button_event_queue_, &event, 2);
    }
    else if(button_buffer < 1000 && mode == 1){
        button_event_t event{
        .type = MiscMuxChanMap::PROGRAM,
        .value = 0
        };
        xQueueSend(*button_event_queue_, &event, 2);
    }
    xSemaphoreGive(mutex_);
}

void PeripheralMonitor::task_read_buttons(void* params){
    PeripheralMonitor* pmon = static_cast<PeripheralMonitor*>(params);
    uint8_t notify_val;

    for(;;){
        if(xQueueReceive(button_interrupt_queue_, &notify_val, 5)){
            pmon->read_buttons();
        }
    }
}



void PeripheralMonitor::read_buttons(){
    int button_buffer;
    // Mutex used to exclusively used mux_ and button_event_queue_
    if(xSemaphoreTake(mutex_, 5) == pdFALSE) return;
    // Scan the 6 main buttons
    uint8_t start = uint8_t(MiscMuxChanMap::OCTAVE_UP);
    for(uint8_t i = start; i > (start - 6); i--){
        mux_->get_value(
            std::move(MuxController::MUX_IDX::MUX_MISC),
            uint8_t(i),
            button_buffer
        );
        if(button_buffer > 3000){
            button_event_t event{
                .type = MiscMuxChanMap(i),
                .value = 1
            };
            xQueueSend(*button_event_queue_, &event, 2);
        }
    }
    xSemaphoreGive(mutex_);
}

constexpr uint8_t adc_raw_to_note_dur(int adc_val){
    // This allows setting the note duration between 50 and 2050 ms
    float conv = 5 + (float(adc_val)/4095.0*200);
    return uint8_t(conv);
}

void PeripheralMonitor::scan_cc_(){
    // Mutex used to exclusively use mux_ and button_event_queue_
    if (xSemaphoreTake(mutex_, 5) == pdFALSE) return;
    mux_->get_value(std::move(MuxController::MUX_IDX::MUX_MISC), uint8_t(MiscMuxChanMap::NOTE_DUR), note_duration_buffer_);

    if(outside_cc_thresh(current_note_duration_raw_, note_duration_buffer_)){
        current_note_duration_raw_ = note_duration_buffer_;
        button_event_t event{
            .type = MiscMuxChanMap::NOTE_DUR,
            .value = adc_raw_to_note_dur(note_duration_buffer_)
        };
        xQueueSend(*button_event_queue_, &event, 2);
        xSemaphoreGive(mutex_);
        return;
    }

    for(uint8_t i = 0; i < 3; i++){
        //ESP_LOGI("PM", "Measure from %d", 15 - i);
        mux_->get_value(std::move(MuxController::MUX_IDX::MUX_MISC), (MiscMuxChanMap::CC1) - i, cc_buffer_[i]);
        //ESP_LOGI("PM", "Got %d", buff[i]);
    }
    xSemaphoreGive(mutex_);
    for(int i = 0; i < 3; i++){
        if(outside_cc_thresh(current_cc_vals_[i], cc_buffer_[i])){
            cc_event_t event;
            // mode == PLAY: Trigger cc event, update current cc vals
            if(mode == 0){
                current_cc_vals_[i] = cc_buffer_[i];
                event.cc_channel = cc_channels_[i];
                event.value = calc_raw_to_midi(current_cc_vals_[i]);
            }
            // mode == PROGRAM: Trigger cc event, update cc channels
            // Channel calculation is exactly the same
            else{
                current_cc_vals_[i] = cc_buffer_[i];
                cc_channels_[i] = calc_raw_to_midi(current_cc_vals_[i]);
                event.cc_channel = i;
                event.value = cc_channels_[i];
            }
            //ESP_LOGI("cc", "cc monitor event val %d, raw: %d, idx: %d", event.value, current_cc_vals_[i], i);
            xQueueSend(*cc_event_queue_, (void*) &event, 2);
        } // end if
    } // end for loop
}

PeripheralMonitor::PeripheralMonitor(MuxController* mux, QueueHandle_t* cc_queue, QueueHandle_t* button_queue, QueueHandle_t* js_queue)
: mux_(mux), cc_event_queue_(cc_queue), button_event_queue_(button_queue), js_event_queue_(js_queue){
    mutex_ = xSemaphoreCreateMutex();
};

void PeripheralMonitor::start(){
    button_interrupt_queue_ = xQueueCreate(20, sizeof(uint8_t));

    xTaskCreate(
        &task_monitor_cc,
        "monitor cc",
        monitor_cc_stack_size_,
        (void*) this,
        (UBaseType_t) 6,
        NULL
    );

    xTaskCreate(
        &task_read_buttons,
        "read buttons",
        monitor_cc_stack_size_,
        (void*) this,
        (UBaseType_t) 3,
        &button_task_handle
    );

    xTaskCreate(
        &task_monitor_switches,
        "monitor switches",
        4096,
        (void*) this,
        (UBaseType_t) 2,
        NULL
    );

    xTaskCreate(
        &task_monitor_js,
        "monitor joystick",
        2048,
        (void*) this,
        (UBaseType_t) 8,
        NULL
    );
}