#include "m_xylo.hpp"
#include "esp_timer.h"
#include <vector>
#include "rom/ets_sys.h"
int timer_count = 0;
int pad_mux_to_read = 0;

typedef MuxController::MUX_IDX mux_types;

QueueHandle_t MXylo::cc_event_queue_;
QueueHandle_t MXylo::button_event_queue_;
QueueHandle_t MXylo::js_event_queue_;

/*
* TIMER CALLBACKS
*/

void IRAM_ATTR MXylo::timer_cb_button_reset(void* params){
    MXylo::instance().button_timeout = false;
}

//
void MXylo::timer_cb_pad_hit(void*){

    uint8_t pad_status;
    MXylo::instance().pad_status.get_status(pad_status);
    if(pad_status == 0){
        MXylo::instance().current_scan_iteration = 0;
        MXylo::instance().note_controller_->clear_rescan();
        return;
    }
    // gpio_set_level((gpio_num_t) 21, 1);
    for(int i = 0; i < 3; i++){
        if(pad_status & 1){
            MXylo::instance().note_controller_->on_pad_hit(i);
        }
        pad_status = pad_status >> 1;
    }

    esp_timer_start_once(MXylo::instance().timer_rescan, MXylo::instance().subsequent_timer_delay_us);
}

void MXylo::timer_cb_rescan(void* params){
    MXylo::instance().current_scan_iteration -= 1;
    // Finished scanning, reset status/saved pads in NC
    if(MXylo::instance().current_scan_iteration <= 0){
        //ESP_LOGI("main", "Rescan end");
        MXylo::instance().pad_status.reset_status();
        MXylo::instance().note_controller_->clear_rescan();
        return;
    }

    uint8_t pad_status;
    MXylo::instance().pad_status.get_status(pad_status);
    if(pad_status == 0){
        //ESP_LOGI("main", "Rescan end");
        MXylo::instance().current_scan_iteration = 0;
        MXylo::instance().note_controller_->clear_rescan();
        return;
    }
    for(int i = 0; i < 3; i++){
        if(pad_status & 1){
            MXylo::instance().note_controller_->on_rescan(i);
        }
        pad_status = pad_status >> 1;
    }
    esp_timer_start_once(MXylo::instance().timer_rescan, MXylo::instance().subsequent_timer_delay_us);
}

/*
* INTERRUPT HANDLERS
*/

const uint8_t MXylo::PADSET_0;
const uint8_t MXylo::PADSET_1;
const uint8_t MXylo::PADSET_2;
void IRAM_ATTR pad_0_int_handler(void* arg){
    if(MXylo::instance().mode_ != MXylo::Mode::PLAY) return;
    int i = int(arg);

    // Short spin before checking GPIO level, so that it stabilizes
    ets_delay_us(10);
    int level = gpio_get_level(gpio_num_t(i));

    uint8_t padset = 0;
    if(i == CONFIG_PAD_SET_0_INT_PIN) padset = MXylo::PADSET_0;
    else if(i == CONFIG_PAD_SET_1_INT_PIN) padset = MXylo::PADSET_1;
    else if(i == CONFIG_PAD_SET_2_INT_PIN) padset = MXylo::PADSET_2;

    if(level < 1){
        MXylo::instance().pad_status.reset_padset(padset);
        return;
    }

    MXylo::instance().pad_status.set_padset(padset);
    MXylo::instance().current_scan_iteration = MXylo::instance().max_pad_scan_iterations;
    if(esp_timer_is_active(MXylo::instance().timer_pad_hit)){
        // If current_scan_iteration is positive, we are still scanning an old signal so just extend the
        // scans and return
        return;
    }
    else{
        esp_timer_start_once(MXylo::instance().timer_pad_hit, MXylo::instance().initial_timer_delay_us);
    }
}


void MXylo::button_int_handler(void* arg){
    MXylo* mxylo = static_cast<MXylo*>(arg);
    // Short spin before checking GPIO level, so that it stabilizes
    ets_delay_us(10);
    // If button is at timeout, ignore (software debouncing)
    // If gpio is low, user has stopped pressing so ignore
    if(mxylo->button_timeout || gpio_get_level(gpio_num_t(CONFIG_BUTTON_INT_PIN)) == 0) return;

    BaseType_t xHigherPrioWoken = pdFALSE;
    static uint8_t val = 1;
    xQueueSendFromISR(PeripheralMonitor::button_interrupt_queue_, &val, &xHigherPrioWoken);
    mxylo->button_timeout = true;
    esp_timer_start_once(mxylo->timer_button_timeout, 200000);
}

/*
* INITIALIZATION
*/

MXylo::MXylo(){
    ESP_LOGI("MX", "M-xylo init");
    std::vector<adc_channel_t> channels = {
        adc_channel_t(CONFIG_XMIDI_CHAN_MUX_PADS_0),
        adc_channel_t(CONFIG_XMIDI_CHAN_MUX_PADS_1),
        adc_channel_t(CONFIG_XMIDI_CHAN_MUX_MISC),
        adc_channel_t(3)
    };


    adc_ctrl_.init_adc(std::move(channels));

    mux_pin_config mux_p0_pins = {
        .S0 = gpio_num_t(CONFIG_MUX_P0_PIN_S0),
        .S1 = gpio_num_t(CONFIG_MUX_P0_PIN_S1),
        .S2 = gpio_num_t(CONFIG_MUX_P0_PIN_S2),
        .S3 = gpio_num_t(CONFIG_MUX_P0_PIN_S3),
    };
    mux_pin_config mux_p1_pins = {
        .S0 = gpio_num_t(CONFIG_MUX_P1_PIN_S0),
        .S1 = gpio_num_t(CONFIG_MUX_P1_PIN_S1),
        .S2 = gpio_num_t(CONFIG_MUX_P1_PIN_S2),
        .S3 = gpio_num_t(CONFIG_MUX_P1_PIN_S3),
    };
    mux_pin_config mux_misc_pins = {
        .S0 = gpio_num_t(CONFIG_MUX_MISC_PIN_S0),
        .S1 = gpio_num_t(CONFIG_MUX_MISC_PIN_S1),
        .S2 = gpio_num_t(CONFIG_MUX_MISC_PIN_S2),
        .S3 = gpio_num_t(CONFIG_MUX_MISC_PIN_S3),
    };

    mux_controller_ = new MuxController(&adc_ctrl_, &mux_p0_pins, &mux_p1_pins, &mux_misc_pins);

    esp_timer_create_args_t args = {
        .callback = &timer_cb_pad_hit,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "play_note_cb"
    };

    ESP_ERROR_CHECK(esp_timer_create(&args, &timer_pad_hit));

    args = {
        .callback = &timer_cb_button_reset,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "button_timeout"
    };

    ESP_ERROR_CHECK(esp_timer_create(&args, &timer_button_timeout));

    args = {
        .callback = &timer_cb_rescan,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "rescan"
    };

    ESP_ERROR_CHECK(esp_timer_create(&args, &timer_rescan));

    note_controller_ = new NoteController{mux_controller_, &usb_midi_, channel_};
    note_controller_->set_note_length(20);

    cc_event_queue_ = xQueueCreate(10, sizeof(PeripheralMonitor::cc_event_t));
    js_event_queue_ = xQueueCreate(20, sizeof(PeripheralMonitor::js_event_t));
    button_event_queue_ = xQueueCreate(5, sizeof(PeripheralMonitor::button_event_t));
    periph_monitor_ = new PeripheralMonitor(mux_controller_, &cc_event_queue_, &button_event_queue_, &js_event_queue_);
    display_.start_display();
}

void MXylo::start(){
    ESP_LOGI("MX", "Start");
    gpio_config_t debug_gpio = {
        .pin_bit_mask = (1 << 21),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE
    };
    // Configure interrupts + create scan pads (0-15) task
    gpio_config_t int_config = {
        .pin_bit_mask = (
            1 << CONFIG_PAD_SET_0_INT_PIN) |
            (1 << CONFIG_PAD_SET_1_INT_PIN) | 
            (1 << CONFIG_BUTTON_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };

    gpio_config(&int_config);
    gpio_config(&debug_gpio);
    //gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_EDGE | ESP_INTR_FLAG_IRAM ));
    gpio_isr_handler_add(
        (gpio_num_t) CONFIG_PAD_SET_0_INT_PIN,
        pad_0_int_handler,
        (void*) CONFIG_PAD_SET_0_INT_PIN
    );

    gpio_isr_handler_add(
        (gpio_num_t) CONFIG_PAD_SET_1_INT_PIN,
        pad_0_int_handler,
        (void*) CONFIG_PAD_SET_1_INT_PIN
    );


    gpio_isr_handler_add(
        (gpio_num_t) CONFIG_BUTTON_INT_PIN,
        button_int_handler,
        (void*) this
    );

    xTaskCreate(&task_process_buttons_, "button events", 4096, (void*) this, (UBaseType_t) 4, NULL);
    xTaskCreate(&task_process_cc_, "cc events", 2048, (void*) this, (UBaseType_t) 7, NULL);
    xTaskCreate(&task_process_js_, "js events", 4096, (void*) this, (UBaseType_t) 9, NULL);
    periph_monitor_->start();
}


/*
* TASKS
*/

void MXylo::task_process_buttons_(void* params){
    MXylo* mxylo = static_cast<MXylo*>(params);
    PeripheralMonitor::button_event_t event_buffer;
    for(;;){
        if(xQueueReceive(mxylo->button_event_queue_, &event_buffer, portMAX_DELAY)){
            // ESP_LOGI("Button events", "type:%d ", uint8_t(event_buffer.type));
            if(event_buffer.type == PeripheralMonitor::MiscMuxChanMap::PROGRAM){
                mxylo->switch_mode(event_buffer.value);
            }
            // Do button/switch/note duration action
            else if(event_buffer.type == PeripheralMonitor::MiscMuxChanMap::NOTE_DUR){
                mxylo->note_controller_->set_note_length(event_buffer.value);
                //ESP_LOGI("Button events", "Setting note length to: %d", event_buffer.value);
            }

            else if(mxylo->mode_ == Mode::PLAY){
                mxylo->play_button_action_(std::move(event_buffer.type));
            }

            else if(mxylo->mode_ == Mode::PROGRAM){
                mxylo->program_button_action_(std::move(event_buffer.type));
            }

        }
    }
}

void MXylo::task_process_js_(void* params){
    MXylo* mxylo = static_cast<MXylo*>(params);
    PeripheralMonitor::js_event_t event_buffer;
    for(;;){
        if(xQueueReceive(js_event_queue_, &event_buffer, portMAX_DELAY) == pdTRUE){
            if(event_buffer.type == PeripheralMonitor::MiscMuxChanMap::JS_MOD){
                // Send mod msg
                mxylo->usb_midi_.send(midi::msg::cc(mxylo->channel_, 1, event_buffer.value));
            }
            else{
                uint8_t low_b = uint8_t(event_buffer.value & 0b01111111);
                uint8_t high_b = uint8_t((event_buffer.value >> 7) & 0b01111111);
                // Send pitch msg
                mxylo->usb_midi_.send(midi::msg::pitch(mxylo->channel_, low_b, high_b));
            }
        }
    }
}


void MXylo::task_process_cc_(void* params){
    MXylo* mxylo = static_cast<MXylo*>(params);
    PeripheralMonitor::cc_event_t event_buffer;
    for(;;){
        xQueueReceive(mxylo->cc_event_queue_, &event_buffer, portMAX_DELAY);
        if(mxylo->mode_ == MXylo::Mode::PROGRAM){
            mxylo->display_.push_event(
                Display::DISP_EVENT::CONTROL_CHANGE_CHANNEL,
                std::move(Display::event_data_t{event_buffer.cc_channel, event_buffer.value})
            );
        }
        else{
                mxylo->usb_midi_.send(midi::msg::cc(mxylo->channel_, event_buffer.cc_channel, event_buffer.value));
            // ESP_LOGI("main", "Send cc");
                mxylo->display_.push_event(
                Display::DISP_EVENT::CONTROL_CHANGE,
                std::move(Display::event_data_t{event_buffer.cc_channel, event_buffer.value})
            );

        }
    }
}

/*
* BUTTON EVENT HANDLERS
*/

void MXylo::play_button_action_(PeripheralMonitor::MiscMuxChanMap&& button){
            switch(button){
                case(PeripheralMonitor::MiscMuxChanMap::OCTAVE_UP):
                    {
                        if(octave_ == 3) return;
                        octave_++;
                        note_controller_->set_octave(octave_);
                        Display::event_data_t data{octave_, 1};
                        display_.push_event(Display::DISP_EVENT::OCTAVE, std::move(data));
                        break;
                    }

                case(PeripheralMonitor::MiscMuxChanMap::OCTAVE_DOWN):
                    {
                        if(octave_ == -3) return;
                        octave_--;
                        note_controller_->set_octave(octave_);
                        Display::event_data_t data{octave_, 0};
                        display_.push_event(Display::DISP_EVENT::OCTAVE, std::move(data));
                        break;
                    }

                case(PeripheralMonitor::MiscMuxChanMap::TRANSPOSE_UP):
                    {
                        if(transpose_ >= 11) return;
                        transpose_++;
                        note_controller_->set_transpose(transpose_);
                        Display::event_data_t data{transpose_, 1};
                        display_.push_event(Display::DISP_EVENT::TRANSPOSE, std::move(data));
                        break;
                    }

                case(PeripheralMonitor::MiscMuxChanMap::TRANSPOSE_DOWN):
                    {
                        if(transpose_ <= -11) return;
                        transpose_--;
                        note_controller_->set_transpose(transpose_);
                        Display::event_data_t data{transpose_, 0};
                        display_.push_event(Display::DISP_EVENT::TRANSPOSE, std::move(data));
                        break;
                    }

                case(PeripheralMonitor::MiscMuxChanMap::PROGRAM_CHANGE_UP):
                    {
                        if(patch_ == 127) return;
                        patch_++;
                        Display::event_data_t data{static_cast<uint8_t>(patch_), 1};
                        display_.push_event(Display::DISP_EVENT::PROGRAM_CHANGE, std::move(data));
                        usb_midi_.send(midi::msg::pc(channel_, patch_));
                        break;
                    }

                case(PeripheralMonitor::MiscMuxChanMap::PROGRAM_CHANGE_DOWN):
                    {
                        if(patch_ == 0) return;
                        patch_--;
                        Display::event_data_t data{static_cast<uint8_t>(patch_), 0};
                        display_.push_event(Display::DISP_EVENT::PROGRAM_CHANGE, std::move(data));
                        usb_midi_.send(midi::msg::pc(channel_, patch_));
                        break;
                    }

            default:
                return;
            }

}

void MXylo::program_button_action_(PeripheralMonitor::MiscMuxChanMap&& button){
    // Change channel
    if(button == PeripheralMonitor::MiscMuxChanMap::OCTAVE_UP || button == PeripheralMonitor::MiscMuxChanMap::TRANSPOSE_UP || button == PeripheralMonitor::MiscMuxChanMap::PROGRAM_CHANGE_UP){
        if(channel_ == 16) return;
        channel_++;
        display_.push_event(Display::DISP_EVENT::CHANNEL_CHANGE, Display::event_data_t{static_cast<uint8_t>(channel_ - 1), 1});
    }

    else if(button == PeripheralMonitor::MiscMuxChanMap::OCTAVE_DOWN || button == PeripheralMonitor::MiscMuxChanMap::TRANSPOSE_DOWN || button == PeripheralMonitor::MiscMuxChanMap::PROGRAM_CHANGE_DOWN){
        if(channel_ == 1) return;
        channel_--;
        display_.push_event(Display::DISP_EVENT::CHANNEL_CHANGE, Display::event_data_t{static_cast<uint8_t>(channel_ - 1), 0});

    }
}

void MXylo::switch_mode(uint8_t val){
    if(val){
        mode_ = Mode::PROGRAM;
    }
    else{
        mode_ = Mode::PLAY;
    }
    periph_monitor_->mode = val;
    display_.push_event(
        Display::DISP_EVENT::MODE,
        std::move(Display::event_data_t{static_cast<uint8_t>(mode_), 0})
    );
}
