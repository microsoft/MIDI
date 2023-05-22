#include "pch.h"
#include "MidiUmpEndpoint.h"
#include "MidiUmpEndpoint.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    winrt::Windows::Devices::Midi2::MidiMessageReadBuffer MidiUmpEndpoint::IncomingMessages()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::MidiMessageWriteBuffer MidiUmpEndpoint::OutcomingMessages()
    {
        throw hresult_not_implemented();
    }
}



// Prototype: Hard-code creating a new network MIDI endpoint
// or maybe just direct to the USB MIDI driver
