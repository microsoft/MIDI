#include "pch.h"
#include "Midi2JitterReductionTimestampMessage.h"
#include "Midi2JitterReductionTimestampMessage.g.cpp"

namespace winrt::Windows::Devices::Midi::implementation
{
    uint16_t Midi2JitterReductionTimestampMessage::SenderClockTimestamp()
    {
        throw hresult_not_implemented();
    }
}
