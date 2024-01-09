// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiOutPort.h"
#include "MidiOutPort.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    hstring MidiOutPort::DeviceId()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiOutPort::SendMessage(winrt::MIDI_ROOT_NAMESPACE_CPP::IMidiMessage const& /*midiMessage*/)
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiOutPort::SendBuffer(winrt::Windows::Storage::Streams::IBuffer const& /*midiData*/)
    {
        throw hresult_not_implemented();
    }
    void MidiOutPort::Close()
    {
        throw hresult_not_implemented();
    }
}
