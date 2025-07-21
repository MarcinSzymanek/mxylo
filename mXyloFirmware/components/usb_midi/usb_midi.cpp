#include <stdio.h>
#include "usb_midi.h"
#include "tinyusb.h"
#include "driver/gpio.h"

namespace usb{

#define GPIO_LED 48
UsbMidi::UsbMidi(){

    s_midi_cfg_desc_handle_ = new std::array<const uint8_t, 101> {
        // Configuration number, interface count, string index, total length, attribute, power in mA
        TUD_CONFIG_DESCRIPTOR(1, usb::UsbMidi::ITF_COUNT, 0, get_total_desc_size(), 0, 100),
        // Interface number, string index, EP Out & EP In address, EP size
        TUD_MIDI_DESCRIPTOR(usb::UsbMidi::ITF_NUM_MIDI, 4, usb::UsbMidi::EPNUM_MIDI, (0x80 | usb::UsbMidi::EPNUM_MIDI), 64),

    };
    tinyusb_config_t const tusb_cfg = {
        .device_descriptor = NULL, // If device_descriptor is NULL, tinyusb_driver_install() will use Kconfig
        .string_descriptor = str_desc.data(),
        .string_descriptor_count = 5,
        .external_phy = false,
#if (TUD_OPT_HIGH_SPEED)
        .fs_configuration_descriptor = s_midi_cfg_desc, // HID configuration descriptor for full-speed and high-speed are the same
        .hs_configuration_descriptor = s_midi_hs_cfg_desc,
        .qualifier_descriptor = NULL,
#else
        .configuration_descriptor = s_midi_cfg_desc_handle_->data(),
#endif // TUD_OPT_HIGH_SPEED
    };
    // gpio_config_t gpio_config_ = {
    //     .pin_bit_mask = (1ULL << GPIO_LED),
    //     .mode = GPIO_MODE_OUTPUT,
    //     .pull_up_en = GPIO_PULLUP_DISABLE,
    //     .pull_down_en = GPIO_PULLDOWN_DISABLE
    // };
    // gpio_config(&gpio_config_);
    // gpio_set_level(gpio_num_t(GPIO_LED), 0);
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    mutex_ = xSemaphoreCreateMutex();
}

void UsbMidi::send(const midi::MidiPacket packet){
    if(tud_midi_mounted()){
        int written = tud_midi_stream_write(0, packet.payload.data(), packet.payload.capacity());
    }
}

constexpr size_t UsbMidi::get_total_desc_size(){
    return TUD_CONFIG_DESC_LEN + CFG_TUD_MIDI * TUD_MIDI_DESC_LEN;
}

} // namespace usb
