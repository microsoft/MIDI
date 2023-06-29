#include "pch.h"
#include "MidiUmpWithTimestamp.h"
#include "MidiUmpWithTimestamp.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    uint64_t MidiUmpWithTimestamp::Timestamp()
    {
        throw hresult_not_implemented();
    }
    void MidiUmpWithTimestamp::Timestamp(uint64_t value)
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiUmp MidiUmpWithTimestamp::Ump()
    {
        throw hresult_not_implemented();
    }
    void MidiUmpWithTimestamp::Ump(winrt::Microsoft::Devices::Midi2::MidiUmp const& value)
    {
        throw hresult_not_implemented();
    }
}
