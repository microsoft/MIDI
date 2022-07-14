#pragma once
#include "Midi2UtilityMessage.g.h"
#include "MidiMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2UtilityMessage : Midi2UtilityMessageT<Midi2UtilityMessage, Windows::Devices::Midi::implementation::MidiMessage>
    {
        Midi2UtilityMessage() = default;

    };
}
