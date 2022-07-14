#pragma once
#include "Midi2PolyPressureMessage.g.h"
#include "Midi2ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2PolyPressureMessage : Midi2PolyPressureMessageT<Midi2PolyPressureMessage, Windows::Devices::Midi::implementation::Midi2ChannelVoiceMessage>
    {
        Midi2PolyPressureMessage() = default;

        uint8_t NoteNumber();
        uint32_t Data();
    };
}
