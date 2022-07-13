#pragma once
#include "MidiStream.g.h"

namespace winrt::Windows::Devices::Midi::implementation
{
    struct MidiStream : MidiStreamT<MidiStream>
    {
        MidiStream() = default;

        hstring TempFoo();
    };
}
namespace winrt::Windows::Devices::Midi::factory_implementation
{
    struct MidiStream : MidiStreamT<MidiStream, implementation::MidiStream>
    {
    };
}
