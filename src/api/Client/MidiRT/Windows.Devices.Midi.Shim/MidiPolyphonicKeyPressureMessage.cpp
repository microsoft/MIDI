// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiPolyphonicKeyPressureMessage.h"
#include "MidiPolyphonicKeyPressureMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    _Use_decl_annotations_
    MidiPolyphonicKeyPressureMessage::MidiPolyphonicKeyPressureMessage(uint8_t /*channel*/, uint8_t /*note*/, uint8_t /*pressure*/)
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiPolyphonicKeyPressureMessage::Channel()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiPolyphonicKeyPressureMessage::Note()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiPolyphonicKeyPressureMessage::Pressure()
    {
        throw hresult_not_implemented();
    }
    foundation::TimeSpan MidiPolyphonicKeyPressureMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    midi1::MidiMessageType MidiPolyphonicKeyPressureMessage::Type()
    {
        throw hresult_not_implemented();
    }
    streams::IBuffer MidiPolyphonicKeyPressureMessage::RawData()
    {
        throw hresult_not_implemented();
    }

}
