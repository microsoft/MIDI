// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiTimeCodeMessage.g.h"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    struct MidiTimeCodeMessage : MidiTimeCodeMessageT<MidiTimeCodeMessage>
    {
        MidiTimeCodeMessage() = default;

        MidiTimeCodeMessage(_In_ uint8_t frameType, _In_ uint8_t values);

        uint8_t FrameType() { return m_frameType; }
        uint8_t Values() { return m_values; }


        foundation::TimeSpan Timestamp() { return m_timestamp; }
        midi1::MidiMessageType Type() { return m_type; }
        streams::IBuffer RawData();

    private:
        foundation::TimeSpan m_timestamp{};
        midi1::MidiMessageType m_type{ midi1::MidiMessageType::None };

        uint8_t m_frameType{};
        uint8_t m_values{};

    };
}
namespace winrt::MIDI_ROOT_NAMESPACE_CPP::factory_implementation
{
    struct MidiTimeCodeMessage : MidiTimeCodeMessageT<MidiTimeCodeMessage, implementation::MidiTimeCodeMessage>
    {
    };
}
