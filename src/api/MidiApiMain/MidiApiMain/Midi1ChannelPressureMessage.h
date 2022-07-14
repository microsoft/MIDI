#pragma once
#include "Midi1ChannelPressureMessage.g.h"
#include "Midi1ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi1ChannelPressureMessage : Midi1ChannelPressureMessageT<Midi1ChannelPressureMessage, Windows::Devices::Midi::implementation::Midi1ChannelVoiceMessage>
    {
        Midi1ChannelPressureMessage() = default;

        uint8_t Data();
    };
}
