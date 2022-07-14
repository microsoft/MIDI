#pragma once
#include "Midi2JitterReductionClockMessage.g.h"
#include "Midi2UtilityMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2JitterReductionClockMessage : Midi2JitterReductionClockMessageT<Midi2JitterReductionClockMessage, Windows::Devices::Midi::implementation::Midi2UtilityMessage>
    {
        Midi2JitterReductionClockMessage() = default;

        uint16_t SenderClockTime();
    };
}
