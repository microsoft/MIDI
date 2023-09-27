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

    _Use_decl_annotations_
    midi2::MidiGroup MidiUmpUtility::GetGroup(_In_ uint32_t const word0)
    {
        return MidiGroup(internal::GetGroupIndexFromFirstWord(word0));
    }


    // It's expected for the user to check to see if the message type has a group field before calling this
    _Use_decl_annotations_
    uint32_t MidiUmpUtility::ReplaceGroup(uint32_t const word0, midi2::MidiGroup const newGroup) noexcept
    {
        return internal::GetFirstWordWithNewGroup(word0, newGroup.Index());
    }

    _Use_decl_annotations_
    bool MidiUmpUtility::MessageTypeHasGroupField(midi2::MidiUmpMessageType const messageType) noexcept
    {
        return internal::MessageTypeHasGroupField((uint8_t)messageType);
    }


    _Use_decl_annotations_
    midi2::MidiChannel MidiUmpUtility::GetChannel(_In_ uint32_t const word0)
    {
        return MidiChannel(internal::GetChannelIndexFromFirstWord(word0));
    }


    // It's expected for the user to check to see if the message type has a channel field before calling this
    _Use_decl_annotations_
    uint32_t MidiUmpUtility::ReplaceChannel(uint32_t const word0, midi2::MidiChannel const newChannel) noexcept
    {
        return internal::GetFirstWordWithNewChannel(word0, newChannel.Index());
    }

    _Use_decl_annotations_
        bool MidiUmpUtility::MessageTypeHasChannelField(midi2::MidiUmpMessageType const messageType) noexcept
    {
        return internal::MessageTypeHasChannelField((uint8_t)messageType);
    }

}
