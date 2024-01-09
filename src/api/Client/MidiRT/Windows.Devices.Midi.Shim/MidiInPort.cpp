// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiInPort.h"
#include "MidiInPort.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    hstring MidiInPort::DeviceId()
    {
        throw hresult_not_implemented();
    }
    winrt::event_token MidiInPort::MessageReceived(winrt::Windows::Foundation::TypedEventHandler<winrt::MIDI_ROOT_NAMESPACE_CPP::MidiInPort, winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageReceivedEventArgs> const& handler)
    {
        throw hresult_not_implemented();
    }
    void MidiInPort::MessageReceived(winrt::event_token const& token) noexcept
    {
        //throw hresult_not_implemented();
    }
    void MidiInPort::Close()
    {
        throw hresult_not_implemented();
    }
}
