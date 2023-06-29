#pragma once
#include "MidiUmp.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiUmp : MidiUmpT<MidiUmp>
    {
        MidiUmp() = default;

        com_array<uint32_t> Words();
        uint8_t WordCount();
        winrt::Microsoft::Devices::Midi2::MidiMessageType MessageType();
        void MessageType(winrt::Microsoft::Devices::Midi2::MidiMessageType const& value);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiUmp : MidiUmpT<MidiUmp, implementation::MidiUmp>
    {
    };
}
