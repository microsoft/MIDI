#include "pch.h"
#include "Midi2JitterReductionClockMessage.h"
#include "Midi2JitterReductionClockMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    uint16_t Midi2JitterReductionClockMessage::SenderClockTime()
    {
        throw hresult_not_implemented();
    }
}
