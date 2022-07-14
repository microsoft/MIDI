#pragma once
#include "Midi1NoteOffMessage.g.h"
#include "Midi1ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi1NoteOffMessage : Midi1NoteOffMessageT<Midi1NoteOffMessage, Windows::Devices::Midi::implementation::Midi1ChannelVoiceMessage>
    {
        Midi1NoteOffMessage() = default;

        uint8_t NoteNumber();
        uint8_t Velocity();
    };
}
