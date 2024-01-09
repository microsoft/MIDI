// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiPitchBendChangeMessage.h"
#include "MidiPitchBendChangeMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    _Use_decl_annotations_
    MidiPitchBendChangeMessage::MidiPitchBendChangeMessage(uint8_t /*channel*/, uint16_t /*bend*/)
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiPitchBendChangeMessage::Channel()
    {
        throw hresult_not_implemented();
    }
    uint16_t MidiPitchBendChangeMessage::Bend()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::TimeSpan MidiPitchBendChangeMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageType MidiPitchBendChangeMessage::Type()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Storage::Streams::IBuffer MidiPitchBendChangeMessage::RawData()
    {
        throw hresult_not_implemented();
    }
}
