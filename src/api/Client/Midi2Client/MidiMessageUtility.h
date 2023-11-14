// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiMessageUtility.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessageUtility
    {
        MidiMessageUtility() = default;

        static bool ValidateMessage32MessageType(_In_ uint32_t const word0) noexcept;
        static bool ValidateMessage64MessageType(_In_ uint32_t const word0) noexcept;
        static bool ValidateMessage96MessageType(_In_ uint32_t const word0) noexcept;
        static bool ValidateMessage128MessageType(_In_ uint32_t const word0) noexcept;

        static midi2::MidiMessageType GetMessageTypeFromMessageFirstWord(_In_ uint32_t const word0) noexcept;
        static midi2::MidiPacketType GetPacketTypeFromMessageFirstWord(_In_ uint32_t const word0) noexcept;



        // This is likely to need a change in logic as new messages are added
        static bool MessageTypeHasGroupField(_In_ midi2::MidiMessageType const messageType) noexcept;
        static uint32_t ReplaceGroupInMessageFirstWord(_In_ uint32_t const word0, _In_ midi2::MidiGroup const newGroup) noexcept;
        static midi2::MidiGroup GetGroupFromMessageFirstWord(_In_ uint32_t const word0);

        // This is likely to need a change in logic as new messages are added
        static bool MessageTypeHasChannelField(_In_ midi2::MidiMessageType const messageType) noexcept;
        static uint32_t ReplaceChannelInMessageFirstWord(_In_ uint32_t const word0, _In_ midi2::MidiChannel const newChannel) noexcept;
        static midi2::MidiChannel GetChannelFromMessageFirstWord(_In_ uint32_t const word0);

        static uint8_t GetFormFromStreamMessageFirstWord(_In_ uint32_t const word0) noexcept;
        static uint16_t GetStatusFromStreamMessageFirstWord(_In_ uint32_t const word0) noexcept;
        static uint8_t GetStatusFromSystemCommonMessage(_In_ uint32_t const word0) noexcept;

        static uint8_t GetStatusFromDataMessage64FirstWord(_In_ uint32_t const word0) noexcept;
        static uint8_t GetNumberOfBytesFromDataMessage64FirstWord(_In_ uint32_t const word0) noexcept;

        static uint8_t GetStatusFromDataMessage128FirstWord(_In_ uint32_t const word0) noexcept;
        static uint8_t GetNumberOfBytesFromDataMessage128FirstWord(_In_ uint32_t const word0) noexcept;


        static winrt::hstring GetMessageFriendlyNameFromFirstWord(_In_ uint32_t const word0) noexcept;


        static uint8_t GetStatusFromUtilityMessage(_In_ uint32_t const word0) noexcept;

        static Midi1ChannelVoiceMessageStatus GetStatusFromMidi1ChannelVoiceMessage(_In_ uint32_t const word0) noexcept;
        static Midi2ChannelVoiceMessageStatus GetStatusFromMidi2ChannelVoiceMessageFirstWord(_In_ uint32_t const word0) noexcept;
        

        static uint8_t GetStatusBankFromFlexDataMessageFirstWord(_In_ uint32_t const word0) noexcept;
        static uint8_t GetStatusFromFlexDataMessageFirstWord(_In_ uint32_t const word0) noexcept;


    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessageUtility : MidiMessageUtilityT<MidiMessageUtility, implementation::MidiMessageUtility, winrt::static_lifetime>
    {
    };
}
