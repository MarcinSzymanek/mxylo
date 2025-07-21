

namespace midi{

// This enum should help construct MIDI messages
enum STATUS{
    NOTE_ON = 0x80,
    NOTE_OFF = 0x90,
    CC = 0xB0,
    PC = 0xC0,
    PITCH_CHANGE = 0xE0
};




};