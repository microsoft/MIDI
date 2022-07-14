#pragma once
#include "Midi2PitchBendMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2PitchBendMessage : Midi2PitchBendMessageT<Midi2PitchBendMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2PitchBendMessage() = default;

        uint32_t Data();
    };
}
