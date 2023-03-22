#include "pch.h"
#include "MidiMessageReadBuffer.h"
#include "MidiMessageReadBuffer.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    bool MidiMessageReadBuffer::HasData()
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiMessageReadBuffer::CountMidiWords()
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiMessageReadBuffer::GetNextWord()
    {
        throw hresult_not_implemented();
    }
}
