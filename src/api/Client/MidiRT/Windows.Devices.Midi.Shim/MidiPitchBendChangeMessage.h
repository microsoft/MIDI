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
        MidiPitchBendChangeMessage(_In_ uint8_t channel, _In_ uint16_t bend);

        uint8_t Channel() { return m_channel; }
        uint16_t Bend() { return m_bend; }


        foundation::TimeSpan Timestamp() { return m_timestamp; }
        midi1::MidiMessageType Type() { return m_type; }
        streams::IBuffer RawData();

    private:
        foundation::TimeSpan m_timestamp{};
        midi1::MidiMessageType m_type{ midi1::MidiMessageType::None };

        uint8_t m_channel{};
        uint16_t m_bend{};
    };
}
namespace winrt::MIDI_ROOT_NAMESPACE_CPP::factory_implementation
{
    struct MidiPitchBendChangeMessage : MidiPitchBendChangeMessageT<MidiPitchBendChangeMessage, implementation::MidiPitchBendChangeMessage>
    {
    };
}
