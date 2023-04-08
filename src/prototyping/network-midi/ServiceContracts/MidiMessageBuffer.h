#pragma once

#include "MidiMessageBuffer.g.h"

namespace winrt::Windows::Devices::Midi::ServiceContracts::implementation
{
    struct MidiMessageBuffer : MidiMessageBufferT<MidiMessageBuffer>
    {
    private:


    public:
        MidiMessageBuffer() = default;

        bool IsFull();
        bool HasData();
        void Clear();

        uint32_t GetNextWord();
        void AddWord(uint32_t word);

    };
}
namespace winrt::Windows::Devices::Midi::ServiceContracts::factory_implementation
{
    struct MidiMessageBuffer : MidiMessageBufferT<MidiMessageBuffer, implementation::MidiMessageBuffer>
    {
    };
}