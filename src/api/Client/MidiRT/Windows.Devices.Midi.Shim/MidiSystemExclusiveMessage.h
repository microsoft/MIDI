// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiSystemExclusiveMessage.g.h"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    struct MidiSystemExclusiveMessage : MidiSystemExclusiveMessageT<MidiSystemExclusiveMessage>
    {
        MidiSystemExclusiveMessage() = default;

        MidiSystemExclusiveMessage(_In_ streams::IBuffer const& rawData);


        foundation::TimeSpan Timestamp() { return m_timestamp; }
        midi1::MidiMessageType Type() { return m_type; }
        streams::IBuffer RawData();

    private:
        foundation::TimeSpan m_timestamp{};
        midi1::MidiMessageType m_type{ midi1::MidiMessageType::None };

    };
}
namespace winrt::MIDI_ROOT_NAMESPACE_CPP::factory_implementation
{
    struct MidiSystemExclusiveMessage : MidiSystemExclusiveMessageT<MidiSystemExclusiveMessage, implementation::MidiSystemExclusiveMessage>
    {
    };
}
