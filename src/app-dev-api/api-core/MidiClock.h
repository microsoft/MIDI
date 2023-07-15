#pragma once
#include "MidiClock.g.h"

#include <midi_timestamp.h>

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiClock : MidiClockT<MidiClock>
    {
        MidiClock() = default;

        static uint64_t GetMidiTimestamp() { return ::Windows::Devices::Midi2::Internal::Shared::GetCurrentMidiTimestamp(); }
        static uint64_t GetMidiTimestampFrequency() { return ::Windows::Devices::Midi2::Internal::Shared::GetMidiTimestampFrequency(); }

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiClock : MidiClockT<MidiClock, implementation::MidiClock>
    {
    };
}
