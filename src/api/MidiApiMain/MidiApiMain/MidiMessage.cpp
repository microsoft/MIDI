#include "pch.h"
#include "MidiMessage.h"
#include "MidiMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    winrt::Windows::Devices::Midi::MidiMessage MidiMessage::FromUmp(winrt::Windows::Devices::Midi::Ump32 const& ump)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::MidiMessage MidiMessage::FromUmp(winrt::Windows::Devices::Midi::Ump64 const& ump)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::MidiMessage MidiMessage::FromUmp(winrt::Windows::Devices::Midi::Ump96 const& ump)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi::MidiMessage MidiMessage::FromUmp(winrt::Windows::Devices::Midi::Ump128 const& ump)
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiMessage::MessageType()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiMessage::Status()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiMessage::Group()
    {
        throw hresult_not_implemented();
    }
}
