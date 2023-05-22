#pragma once
#include "MidiMessageWriteBuffer.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessageWriteBuffer : MidiMessageWriteBufferT<MidiMessageWriteBuffer>
    {
        MidiMessageWriteBuffer() = default;

        bool CanWrite(uint16_t numberOfWords);
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessageWriteBuffer : MidiMessageWriteBufferT<MidiMessageWriteBuffer, implementation::MidiMessageWriteBuffer>
    {
    };
}
