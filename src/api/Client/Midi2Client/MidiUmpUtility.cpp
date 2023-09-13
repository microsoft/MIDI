// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiUmpUtility.h"
#include "MidiUmpUtility.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    bool MidiUmpUtility::ValidateUmp32MessageType(uint32_t const word0) noexcept
    {
        return internal::GetUmpLengthInMidiWordsFromFirstWord(word0) == 1;
    }

    _Use_decl_annotations_
    bool MidiUmpUtility::ValidateUmp64MessageType(uint32_t const word0) noexcept
    {
        return internal::GetUmpLengthInMidiWordsFromFirstWord(word0) == 2;
    }

    _Use_decl_annotations_
    bool MidiUmpUtility::ValidateUmp96MessageType(uint32_t const word0) noexcept
    {
        return internal::GetUmpLengthInMidiWordsFromFirstWord(word0) == 3;
    }

    _Use_decl_annotations_
    bool MidiUmpUtility::ValidateUmp128MessageType(uint32_t const word0) noexcept
    {
        return internal::GetUmpLengthInMidiWordsFromFirstWord(word0) == 4;
    }

    _Use_decl_annotations_
    midi2::MidiUmpMessageType MidiUmpUtility::GetMessageTypeFromFirstUmpWord(uint32_t const word0) noexcept
    {
        return (midi2::MidiUmpMessageType)internal::GetUmpMessageTypeFromFirstWord(word0);
    }

    _Use_decl_annotations_
    midi2::MidiUmpPacketType MidiUmpUtility::GetPacketTypeFromFirstUmpWord(uint32_t const word0) noexcept
    {
        return (midi2::MidiUmpPacketType)internal::GetUmpLengthInMidiWordsFromFirstWord(word0);
    }
}
