#pragma once
#include "Midi1NoteOnMessage.g.h"
#include "Midi1ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi1NoteOnMessage : Midi1NoteOnMessageT<Midi1NoteOnMessage, Windows::Devices::Midi::implementation::Midi1ChannelVoiceMessage>
    {
        Midi1NoteOnMessage() = default;

        uint8_t NoteNumber();
        uint8_t Velocity();
    };
}
