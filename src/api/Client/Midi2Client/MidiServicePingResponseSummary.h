#pragma once
#include "MidiServicePingResponseSummary.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiServicePingResponseSummary : MidiServicePingResponseSummaryT<MidiServicePingResponseSummary>
    {
        MidiServicePingResponseSummary() = default;

        uint64_t TotalPingRoundTripMidiClock();
        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::MidiServicePingResponse> Responses();
    };
}
