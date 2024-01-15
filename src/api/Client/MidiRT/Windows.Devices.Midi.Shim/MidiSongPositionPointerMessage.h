// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiSongPositionPointerMessage.g.h"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    struct MidiSongPositionPointerMessage : MidiSongPositionPointerMessageT<MidiSongPositionPointerMessage>
    {
        MidiSongPositionPointerMessage() = default;

        MidiSongPositionPointerMessage(_In_ uint16_t beats);

        uint16_t Beats() { return m_beats; }


        foundation::TimeSpan Timestamp() { return m_timestamp; }
        midi1::MidiMessageType Type() { return m_type; }
        streams::IBuffer RawData();

    private:
        foundation::TimeSpan m_timestamp{};
        midi1::MidiMessageType m_type{ midi1::MidiMessageType::None };

        uint16_t m_beats{};
    };
}
namespace winrt::MIDI_ROOT_NAMESPACE_CPP::factory_implementation
{
    struct MidiSongPositionPointerMessage : MidiSongPositionPointerMessageT<MidiSongPositionPointerMessage, implementation::MidiSongPositionPointerMessage>
    {
    };
}
