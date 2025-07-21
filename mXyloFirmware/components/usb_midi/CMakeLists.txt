idf_component_register(SRCS "usb_midi.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES "esp_tinyusb" "tinyusb" "midi_msg" "driver")
