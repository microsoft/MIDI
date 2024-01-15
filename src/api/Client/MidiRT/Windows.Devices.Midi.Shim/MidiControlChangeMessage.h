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

        uint8_t Channel() { return m_channel; }
        uint8_t Controller() { return m_controller; }
        uint8_t ControlValue() { return m_controlValue; }


        foundation::TimeSpan Timestamp() { return m_timestamp; }
        midi1::MidiMessageType Type() { return m_type; }
        streams::IBuffer RawData();

    private:
        foundation::TimeSpan m_timestamp{};
        midi1::MidiMessageType m_type{ midi1::MidiMessageType::None };

        uint8_t m_channel{};
        uint8_t m_controller{};
        uint8_t m_controlValue{};
    };
}
namespace winrt::MIDI_ROOT_NAMESPACE_CPP::factory_implementation
{
    struct MidiControlChangeMessage : MidiControlChangeMessageT<MidiControlChangeMessage, implementation::MidiControlChangeMessage>
    {
    };
}
