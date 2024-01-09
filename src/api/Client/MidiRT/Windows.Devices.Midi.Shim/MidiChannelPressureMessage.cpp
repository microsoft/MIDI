// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiChannelPressureMessage.h"
#include "MidiChannelPressureMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    MidiChannelPressureMessage::MidiChannelPressureMessage(uint8_t channel, uint8_t pressure)
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiChannelPressureMessage::Channel()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiChannelPressureMessage::Pressure()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::TimeSpan MidiChannelPressureMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageType MidiChannelPressureMessage::Type()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Storage::Streams::IBuffer MidiChannelPressureMessage::RawData()
    {
        throw hresult_not_implemented();
    }
}
