#include "midi_msg.h"
#include "stdio.h"

namespace midi{

    namespace msg{
        uint8_t make_status_byte(MIDI_STATUS status, uint8_t channel){
            return static_cast<uint8_t>(status | channel);
        }

        MidiPacket noteOn(uint8_t channel, uint8_t note_value, uint8_t velocity){
            return MidiPacket{ make_status_byte(NOTE_ON, channel), note_value, velocity};
        }

        MidiPacket noteOff(uint8_t channel, uint8_t note_value){
            return MidiPacket { make_status_byte(NOTE_OFF, channel), note_value, 100};
        }

        MidiPacket cc(uint8_t channel, uint8_t cc_val, uint8_t val){
            return MidiPacket{ make_status_byte(CC, channel), cc_val, val};
        }

        MidiPacket pc(uint8_t channel, uint8_t pc_val){
            return MidiPacket{ make_status_byte(PC, channel), pc_val};
        }

        MidiPacket pitch(uint8_t channel, uint8_t low_b, uint8_t high_b){
            return MidiPacket{ make_status_byte(PITCH_BEND, channel), low_b, high_b};
        }

    } // namespace msg

}   // namespace midi


