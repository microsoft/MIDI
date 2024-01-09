// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiProgramChangeMessage.h"
#include "MidiProgramChangeMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    MidiProgramChangeMessage::MidiProgramChangeMessage(uint8_t channel, uint8_t program)
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiProgramChangeMessage::Channel()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiProgramChangeMessage::Program()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::TimeSpan MidiProgramChangeMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageType MidiProgramChangeMessage::Type()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Storage::Streams::IBuffer MidiProgramChangeMessage::RawData()
    {
        throw hresult_not_implemented();
    }
}
