#pragma once
#include "Midi1PitchBendMessage.g.h"
#include "Midi1ChannelVoiceMessage.h"

namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi1PitchBendMessage : Midi1PitchBendMessageT<Midi1PitchBendMessage, Windows::Devices::Midi::implementation::Midi1ChannelVoiceMessage>
    {
        Midi1PitchBendMessage() = default;

        uint8_t DataLsb();
        uint8_t DataMsb();
        uint16_t CombinedBendData();
    };
}
