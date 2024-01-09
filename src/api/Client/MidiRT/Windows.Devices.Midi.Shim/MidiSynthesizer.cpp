// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSynthesizer.h"
#include "MidiSynthesizer.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    hstring MidiSynthesizer::DeviceId()
    {
        throw hresult_not_implemented();
    }
    void MidiSynthesizer::SendMessage(winrt::MIDI_ROOT_NAMESPACE_CPP::IMidiMessage const& midiMessage)
    {
        throw hresult_not_implemented();
    }
    void MidiSynthesizer::SendBuffer(winrt::Windows::Storage::Streams::IBuffer const& midiData)
    {
        throw hresult_not_implemented();
    }
}
