#pragma once
#include "Midi2PerNotePitchBendMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2PerNotePitchBendMessage : Midi2PerNotePitchBendMessageT<Midi2PerNotePitchBendMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2PerNotePitchBendMessage() = default;

        uint8_t NoteNumber();
        uint32_t Data();
    };
}
