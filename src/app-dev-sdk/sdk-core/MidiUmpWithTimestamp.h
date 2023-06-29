#pragma once
#include "MidiUmpWithTimestamp.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiUmpWithTimestamp : MidiUmpWithTimestampT<MidiUmpWithTimestamp>
    {
        MidiUmpWithTimestamp() = default;

        static winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp FromUmp32(winrt::Microsoft::Devices::Midi2::MidiUmp32 const& ump);
        static winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp FromUmp64(winrt::Microsoft::Devices::Midi2::MidiUmp64 const& ump);
        static winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp FromUmp96(winrt::Microsoft::Devices::Midi2::MidiUmp96 const& ump);
        static winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp FromUmp128(winrt::Microsoft::Devices::Midi2::MidiUmp128 const& ump);
        uint64_t Timestamp();
        void Timestamp(uint64_t value);
        com_array<uint32_t> Words();
        uint8_t WordCount();
        winrt::Microsoft::Devices::Midi2::MidiUmpMessageType MessageType();
        void MessageType(winrt::Microsoft::Devices::Midi2::MidiUmpMessageType const& value);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiUmpWithTimestamp : MidiUmpWithTimestampT<MidiUmpWithTimestamp, implementation::MidiUmpWithTimestamp>
    {
    };
}
