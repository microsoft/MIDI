#include "pch.h"
#include "MidiUmp.h"
#include "MidiUmp.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    com_array<uint32_t> MidiUmp::Words()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiUmp::WordCount()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiMessageType MidiUmp::MessageType()
    {
        throw hresult_not_implemented();
    }
    void MidiUmp::MessageType(winrt::Microsoft::Devices::Midi2::MidiMessageType const& value)
    {
        throw hresult_not_implemented();
    }
}
