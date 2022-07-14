#pragma once
#include "Midi1ProgramChangeMessage.g.h"
#include "Midi1ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi1ProgramChangeMessage : Midi1ProgramChangeMessageT<Midi1ProgramChangeMessage, Windows::Devices::Midi::implementation::Midi1ChannelVoiceMessage>
    {
        Midi1ProgramChangeMessage() = default;

        uint8_t Program();
    };
}
