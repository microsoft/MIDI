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

        uint8_t Song();
        winrt::Windows::Foundation::TimeSpan Timestamp();
        winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageType Type();
        winrt::Windows::Storage::Streams::IBuffer RawData();
    };
}
namespace winrt::MIDI_ROOT_NAMESPACE_CPP::factory_implementation
{
    struct MidiSongSelectMessage : MidiSongSelectMessageT<MidiSongSelectMessage, implementation::MidiSongSelectMessage>
    {
    };
}
