#include "pch.h"
#include "MidiMessageWriteBuffer.h"
#include "MidiMessageWriteBuffer.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    bool MidiMessageWriteBuffer::CanWrite(uint16_t numberOfWords)
    {
        throw hresult_not_implemented();
    }
}
