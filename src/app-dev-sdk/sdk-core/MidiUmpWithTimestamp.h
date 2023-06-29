#pragma once
#include "MidiUmpWithTimestamp.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiUmpWithTimestamp : MidiUmpWithTimestampT<MidiUmpWithTimestamp>
    {
        MidiUmpWithTimestamp() = default;

        uint64_t Timestamp();
        void Timestamp(uint64_t value);
        winrt::Microsoft::Devices::Midi2::MidiUmp Ump();
        void Ump(winrt::Microsoft::Devices::Midi2::MidiUmp const& value);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiUmpWithTimestamp : MidiUmpWithTimestampT<MidiUmpWithTimestamp, implementation::MidiUmpWithTimestamp>
    {
    };
}
