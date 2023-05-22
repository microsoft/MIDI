#include "pch.h"
#include "MidiSession.h"
#include "MidiSession.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    winrt::Windows::Devices::Midi2::MidiSession MidiSession::CreateSession(hstring const& sessionName)
    {
        throw hresult_not_implemented();
    }
    hstring MidiSession::Id()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::MidiUmpEndpoint MidiSession::ConnectToEndpoint(hstring const& id)
    {

        // Prototype: Hard-code creating a new network MIDI endpoint




    }
}
