#pragma once

#include <array>
#include <initializer_list>
#include <memory>

#include "tinyusb.h"
#include "midi_msg.h"

#include "FreeRTOS.h"
#include "freertos/semphr.h"

namespace usb{

using std::string;

typedef std::array<const uint8_t, 101> midi_cfg_desc_t;

class UsbMidi{
public:
    UsbMidi();

    constexpr size_t get_total_desc_size();
    enum interface_count{
        ITF_NUM_MIDI = 0,
        ITF_NUM_MIDI_STREAMING,
        ITF_COUNT
    };

    enum usb_endpoints{
        EP_EMPTY = 0,
        EPNUM_MIDI
    };
    template<class InputIt>
    constexpr void set_str_desc(InputIt it){
        for(size_t i = 1; i < 5; i++){
            str_desc[i] = it;
            it++;
        }
    }
    void send(const midi::MidiPacket packet);
private:
    std::array<const char*, 5> str_desc = {
        // array of poinbter to string descriptors
        (char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
        "MSzymanek",             // 1: Manufacturer
        "M-Xylo",      // 2: Product
        "123456",              // 3: Serials, should use chip ID
        "M-Xylo", // 4: MIDI
    };

    bool debug_led = false;

    std::array<const uint8_t, 101>* s_midi_cfg_desc_handle_;

    SemaphoreHandle_t mutex_;
};
};
