#pragma once
#include "Midi2ChannelPressureMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2ChannelPressureMessage : Midi2ChannelPressureMessageT<Midi2ChannelPressureMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2ChannelPressureMessage() = default;

        uint32_t Data();
    };
}
