#include "pch.h"
#include "MidiMessagesReceivedEventArgs.h"
#include "MidiMessagesReceivedEventArgs.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    hstring MidiMessagesReceivedEventArgs::SourceMidiEndpointId()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiMessageReader MidiMessagesReceivedEventArgs::GetMessageReader()
    {
        throw hresult_not_implemented();
    }
}
