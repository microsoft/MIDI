#pragma once
#include "Midi1ControlChangeMessage.g.h"
#include "Midi1ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi1ControlChangeMessage : Midi1ControlChangeMessageT<Midi1ControlChangeMessage, Windows::Devices::Midi::implementation::Midi1ChannelVoiceMessage>
    {
        Midi1ControlChangeMessage() = default;

        uint8_t Index();
        uint8_t Data();
    };
}
