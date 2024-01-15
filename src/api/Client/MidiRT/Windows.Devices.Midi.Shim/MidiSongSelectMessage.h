// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiSongSelectMessage.g.h"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    struct MidiSongSelectMessage : MidiSongSelectMessageT<MidiSongSelectMessage>
    {
        MidiSongSelectMessage() = default;

        MidiSongSelectMessage(_In_ uint8_t song);

        uint8_t Song() { return m_song; }


        foundation::TimeSpan Timestamp() { return m_timestamp; }
        midi1::MidiMessageType Type() { return m_type; }
        streams::IBuffer RawData();

    private:
        foundation::TimeSpan m_timestamp{};
        midi1::MidiMessageType m_type{ midi1::MidiMessageType::None };

        uint8_t m_song{};
    };
}
namespace winrt::MIDI_ROOT_NAMESPACE_CPP::factory_implementation
{
    struct MidiSongSelectMessage : MidiSongSelectMessageT<MidiSongSelectMessage, implementation::MidiSongSelectMessage>
    {
    };
}
