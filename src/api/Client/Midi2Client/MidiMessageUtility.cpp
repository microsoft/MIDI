// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageUtility.h"
#include "MidiMessageUtility.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    bool MidiMessageUtility::ValidateMessage32MessageType(uint32_t const word0) noexcept
    {
        return internal::GetUmpLengthInMidiWordsFromFirstWord(word0) == 1;
    }

    _Use_decl_annotations_
    bool MidiMessageUtility::ValidateMessage64MessageType(uint32_t const word0) noexcept
    {
        return internal::GetUmpLengthInMidiWordsFromFirstWord(word0) == 2;
    }

    _Use_decl_annotations_
    bool MidiMessageUtility::ValidateMessage96MessageType(uint32_t const word0) noexcept
    {
        return internal::GetUmpLengthInMidiWordsFromFirstWord(word0) == 3;
    }

    _Use_decl_annotations_
    bool MidiMessageUtility::ValidateMessage128MessageType(uint32_t const word0) noexcept
    {
        return internal::GetUmpLengthInMidiWordsFromFirstWord(word0) == 4;
    }

    _Use_decl_annotations_
    midi2::MidiMessageType MidiMessageUtility::GetMessageTypeFromFirstMessageWord(uint32_t const word0) noexcept
    {
        return (midi2::MidiMessageType)internal::GetUmpMessageTypeFromFirstWord(word0);
    }

    _Use_decl_annotations_
    midi2::MidiPacketType MidiMessageUtility::GetPacketTypeFromFirstMessageWord(uint32_t const word0) noexcept
    {
        return (midi2::MidiPacketType)internal::GetUmpLengthInMidiWordsFromFirstWord(word0);
    }

    _Use_decl_annotations_
    midi2::MidiGroup MidiMessageUtility::GetGroup(_In_ uint32_t const word0)
    {
        return MidiGroup(internal::GetGroupIndexFromFirstWord(word0));
    }


    // It's expected for the user to check to see if the message type has a group field before calling this
    _Use_decl_annotations_
    uint32_t MidiMessageUtility::ReplaceGroup(uint32_t const word0, midi2::MidiGroup const newGroup) noexcept
    {
        return internal::GetFirstWordWithNewGroup(word0, newGroup.Index());
    }

    _Use_decl_annotations_
    bool MidiMessageUtility::MessageTypeHasGroupField(midi2::MidiMessageType const messageType) noexcept
    {
        return internal::MessageTypeHasGroupField((uint8_t)messageType);
    }


    _Use_decl_annotations_
    midi2::MidiChannel MidiMessageUtility::GetChannel(_In_ uint32_t const word0)
    {
        return MidiChannel(internal::GetChannelIndexFromFirstWord(word0));
    }


    // It's expected for the user to check to see if the message type has a channel field before calling this
    _Use_decl_annotations_
    uint32_t MidiMessageUtility::ReplaceChannel(uint32_t const word0, midi2::MidiChannel const newChannel) noexcept
    {
        return internal::GetFirstWordWithNewChannel(word0, newChannel.Index());
    }

    _Use_decl_annotations_
        bool MidiMessageUtility::MessageTypeHasChannelField(midi2::MidiMessageType const messageType) noexcept
    {
        return internal::MessageTypeHasChannelField((uint8_t)messageType);
    }

}
