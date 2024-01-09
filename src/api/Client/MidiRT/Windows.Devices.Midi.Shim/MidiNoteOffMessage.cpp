// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiNoteOffMessage.h"
#include "MidiNoteOffMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    _Use_decl_annotations_
    MidiNoteOffMessage::MidiNoteOffMessage(uint8_t /*channel*/, uint8_t /*note*/, uint8_t /*velocity*/)
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiNoteOffMessage::Channel()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiNoteOffMessage::Note()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiNoteOffMessage::Velocity()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::TimeSpan MidiNoteOffMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageType MidiNoteOffMessage::Type()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Storage::Streams::IBuffer MidiNoteOffMessage::RawData()
    {
        throw hresult_not_implemented();
    }
}
