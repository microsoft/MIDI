#pragma once
#include "Midi2ControlChangeMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"

namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2ControlChangeMessage : Midi2ControlChangeMessageT<Midi2ControlChangeMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2ControlChangeMessage() = default;

        uint8_t Index();
        uint32_t Data();
    };
}
