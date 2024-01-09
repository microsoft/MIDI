// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiTimeCodeMessage.h"
#include "MidiTimeCodeMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    MidiTimeCodeMessage::MidiTimeCodeMessage(uint8_t frameType, uint8_t values)
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiTimeCodeMessage::FrameType()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiTimeCodeMessage::Values()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::TimeSpan MidiTimeCodeMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageType MidiTimeCodeMessage::Type()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Storage::Streams::IBuffer MidiTimeCodeMessage::RawData()
    {
        throw hresult_not_implemented();
    }
}
