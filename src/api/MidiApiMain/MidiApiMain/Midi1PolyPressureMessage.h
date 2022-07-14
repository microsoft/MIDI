#pragma once
#include "Midi1PolyPressureMessage.g.h"
#include "Midi1ChannelVoiceMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi1PolyPressureMessage : Midi1PolyPressureMessageT<Midi1PolyPressureMessage, Windows::Devices::Midi::implementation::Midi1ChannelVoiceMessage>
    {
        Midi1PolyPressureMessage() = default;

        uint8_t NoteNumber();
        uint8_t Data();
    };
}
