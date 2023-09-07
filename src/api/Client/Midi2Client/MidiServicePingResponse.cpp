#include "pch.h"
#include "MidiServicePingResponse.h"
#include "MidiServicePingResponse.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    uint64_t MidiServicePingResponse::Id()
    {
        throw hresult_not_implemented();
    }
    uint64_t MidiServicePingResponse::SendMidiTimestamp()
    {
        throw hresult_not_implemented();
    }
    uint64_t MidiServicePingResponse::ReceiveMidiTimestamp()
    {
        throw hresult_not_implemented();
    }
    uint64_t MidiServicePingResponse::DeltaTimestamp()
    {
        throw hresult_not_implemented();
    }
}
