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
