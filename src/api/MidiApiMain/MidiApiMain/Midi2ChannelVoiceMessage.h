#pragma once
#include "Midi2ChannelVoiceMessage.g.h"
#include "MidiMessage.h"

namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2ChannelVoiceMessage : Midi2ChannelVoiceMessageT<Midi2ChannelVoiceMessage, Windows::Devices::Midi::implementation::MidiMessage>
    {
        Midi2ChannelVoiceMessage() = default;

        uint8_t Channel();
    };
}
