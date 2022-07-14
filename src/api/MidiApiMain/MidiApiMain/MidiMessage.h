#pragma once
#include "MidiMessage.g.h"

namespace winrt::Windows::Devices::Midi::implementation
{
    struct MidiMessage : MidiMessageT<MidiMessage>
    {
        MidiMessage() = default;

        static winrt::Windows::Devices::Midi::MidiMessage FromUmp(winrt::Windows::Devices::Midi::Ump32 const& ump);
        static winrt::Windows::Devices::Midi::MidiMessage FromUmp(winrt::Windows::Devices::Midi::Ump64 const& ump);
        static winrt::Windows::Devices::Midi::MidiMessage FromUmp(winrt::Windows::Devices::Midi::Ump96 const& ump);
        static winrt::Windows::Devices::Midi::MidiMessage FromUmp(winrt::Windows::Devices::Midi::Ump128 const& ump);
        uint8_t MessageType();
        uint8_t Status();
        uint8_t Group();
    };
}
namespace winrt::Windows::Devices::Midi::factory_implementation
{
    struct MidiMessage : MidiMessageT<MidiMessage, implementation::MidiMessage>
    {
    };
}
