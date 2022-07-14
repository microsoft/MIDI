#pragma once
#include "Midi2NoteOffMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2NoteOffMessage : Midi2NoteOffMessageT<Midi2NoteOffMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2NoteOffMessage() = default;

        uint8_t NoteNumber();
        uint8_t AttributeType();
        uint16_t Velocity();
        uint16_t AttributeData();
    };
}
