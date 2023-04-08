#pragma once

#include "MidiMessageBuffer.g.h"

#include <boost/circular_buffer.hpp>

namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::implementation
{
    struct MidiMessageBuffer : MidiMessageBufferT<MidiMessageBuffer>
    {
    private:
        const size_t BufferSize = 1001;

        boost::circular_buffer<uint32_t> _buffer { BufferSize };
        

    public:
        MidiMessageBuffer() = default;

        bool IsFull();
        bool HasData();
        void Clear();

        uint32_t ReadWord();
        void WriteWord(uint32_t word);

    };
}
namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::factory_implementation
{
    struct MidiMessageBuffer : MidiMessageBufferT<MidiMessageBuffer, implementation::MidiMessageBuffer>
    {
    };
}