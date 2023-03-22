#include "pch.h"
#include "MidiMessageBuffer.h"
#include "MidiMessageBuffer.g.cpp"

#include <iostream>

namespace winrt::Windows::Devices::Midi::NetworkMidiTransportPlugin::implementation
{
    bool MidiMessageBuffer::IsFull()
    {
        return _buffer.size() == _buffer.capacity();
    }
    bool MidiMessageBuffer::HasData()
    {
        return _buffer.size() > 0;
    }
    uint32_t MidiMessageBuffer::ReadWord()
    {
        // this needs locking

        auto val = _buffer.front();
        _buffer.pop_front();

        return val;
    }
    void MidiMessageBuffer::WriteWord(uint32_t word)
    {
        // this should also lock
        _buffer.push_back(word);
    }
    void MidiMessageBuffer::Clear()
    {
        // this should also lock
        _buffer.clear();
    }
}