// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiNoteOnMessage.h"
#include "MidiNoteOnMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    _Use_decl_annotations_
    MidiNoteOnMessage::MidiNoteOnMessage(uint8_t /*channel*/, uint8_t /*note*/, uint8_t /*velocity*/)
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiNoteOnMessage::Channel()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiNoteOnMessage::Note()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiNoteOnMessage::Velocity()
    {
        throw hresult_not_implemented();
    }
    foundation::TimeSpan MidiNoteOnMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    midi1::MidiMessageType MidiNoteOnMessage::Type()
    {
        throw hresult_not_implemented();
    }
    streams::IBuffer MidiNoteOnMessage::RawData()
    {
        throw hresult_not_implemented();
    }

}
