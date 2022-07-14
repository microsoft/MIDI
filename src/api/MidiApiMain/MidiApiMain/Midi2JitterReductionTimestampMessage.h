#pragma once
#include "Midi2JitterReductionTimestampMessage.g.h"
#include "Midi2UtilityMessage.h"


namespace winrt::Windows::Devices::Midi::implementation
{
    struct Midi2JitterReductionTimestampMessage : Midi2JitterReductionTimestampMessageT<Midi2JitterReductionTimestampMessage, Windows::Devices::Midi::implementation::Midi2UtilityMessage>
    {
        Midi2JitterReductionTimestampMessage() = default;

        uint16_t SenderClockTimestamp();
    };
}
