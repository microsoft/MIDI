#pragma once
#include "Midi1ChannelVoiceMessage.g.h"
#include "MidiMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi1ChannelVoiceMessage : Midi1ChannelVoiceMessageT<Midi1ChannelVoiceMessage, Windows::Devices::Midi::implementation::MidiMessage>
    {
        Midi1ChannelVoiceMessage() = default;

        uint8_t Channel();
    };
}
