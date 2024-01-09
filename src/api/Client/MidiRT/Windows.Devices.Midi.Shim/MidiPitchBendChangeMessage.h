// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiPitchBendChangeMessage.g.h"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    struct MidiPitchBendChangeMessage : MidiPitchBendChangeMessageT<MidiPitchBendChangeMessage>
    {
        MidiPitchBendChangeMessage() = default;

        MidiPitchBendChangeMessage(uint8_t channel, uint16_t bend);
        uint8_t Channel();
        uint16_t Bend();
        winrt::Windows::Foundation::TimeSpan Timestamp();
        winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageType Type();
        winrt::Windows::Storage::Streams::IBuffer RawData();
    };
}
namespace winrt::MIDI_ROOT_NAMESPACE_CPP::factory_implementation
{
    struct MidiPitchBendChangeMessage : MidiPitchBendChangeMessageT<MidiPitchBendChangeMessage, implementation::MidiPitchBendChangeMessage>
    {
    };
}
