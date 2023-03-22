#include "pch.h"
#include "MidiMessageBuffer.h"
#include "MidiMessageBuffer.g.cpp"

#include <iostream>

namespace winrt::Windows::Devices::Midi::ServiceContracts::implementation
{
    bool MidiMessageBuffer::IsFull()
    {
        throw hresult_not_implemented();
    }
    bool MidiMessageBuffer::HasData()
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiMessageBuffer::GetNextWord()
    {
        throw hresult_not_implemented();
    }
    void MidiMessageBuffer::AddWord(uint32_t word)
    {
        throw hresult_not_implemented();
    }
    void MidiMessageBuffer::Clear()
    {
        throw hresult_not_implemented();
    }
}