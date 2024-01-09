// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiControlChangeMessage.g.h"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    struct MidiControlChangeMessage : MidiControlChangeMessageT<MidiControlChangeMessage>
    {
        MidiControlChangeMessage() = default;

        MidiControlChangeMessage(_In_ uint8_t channel, _In_ uint8_t controller, _In_ uint8_t controlValue);
        uint8_t Channel();
        uint8_t Controller();
        uint8_t ControlValue();
        winrt::Windows::Foundation::TimeSpan Timestamp();
        winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageType Type();
        winrt::Windows::Storage::Streams::IBuffer RawData();
    };
}
namespace winrt::MIDI_ROOT_NAMESPACE_CPP::factory_implementation
{
    struct MidiControlChangeMessage : MidiControlChangeMessageT<MidiControlChangeMessage, implementation::MidiControlChangeMessage>
    {
    };
}
