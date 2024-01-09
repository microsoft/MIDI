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

    _Use_decl_annotations_
    winrt::event_token MidiInPort::MessageReceived(
        foundation::TypedEventHandler<midi1::MidiInPort, midi1::MidiMessageReceivedEventArgs> const& /*handler*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiInPort::MessageReceived(winrt::event_token const& /*token*/) noexcept
    {
        //throw hresult_not_implemented();
    }
    void MidiInPort::Close()
    {
        throw hresult_not_implemented();
    }
}
