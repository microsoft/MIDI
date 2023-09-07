#pragma once
#include "MidiServicePingResponse.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiServicePingResponse : MidiServicePingResponseT<MidiServicePingResponse>
    {
        MidiServicePingResponse() = default;

        uint64_t Id();
        uint64_t SendMidiTimestamp();
        uint64_t ReceiveMidiTimestamp();
        uint64_t DeltaTimestamp();
    };
}
