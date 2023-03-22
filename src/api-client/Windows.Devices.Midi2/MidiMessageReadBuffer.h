#pragma once
#include "MidiMessageReadBuffer.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessageReadBuffer : MidiMessageReadBufferT<MidiMessageReadBuffer>
    {
        MidiMessageReadBuffer() = default;

        bool HasData();
        uint32_t CountMidiWords();
        uint32_t GetNextWord();
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessageReadBuffer : MidiMessageReadBufferT<MidiMessageReadBuffer, implementation::MidiMessageReadBuffer>
    {
    };
}
