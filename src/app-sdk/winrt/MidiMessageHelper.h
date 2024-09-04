// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Messages.MidiMessageHelper.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Messages::implementation
{
    struct MidiMessageHelper
    {
        MidiMessageHelper() = default;

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


        static winrt::hstring GetMessageDisplayNameFromFirstWord(_In_ uint32_t const word0) noexcept;
        static winrt::hstring GetNoteDisplayNameFromNoteIndex(_In_ uint8_t const noteIndex) noexcept;
        
        // these are int16_t becuase IDL doesn't support int8_t
        static int16_t GetNoteOctaveFromNoteIndex(_In_ uint8_t const noteIndex) noexcept;
        static int16_t GetNoteOctaveFromNoteIndex(_In_ uint8_t const noteIndex, _In_ uint8_t middleCOctave) noexcept;

        static uint8_t GetStatusFromUtilityMessage(_In_ uint32_t const word0) noexcept;

        static Midi1ChannelVoiceMessageStatus GetStatusFromMidi1ChannelVoiceMessage(_In_ uint32_t const word0) noexcept;
        static Midi2ChannelVoiceMessageStatus GetStatusFromMidi2ChannelVoiceMessageFirstWord(_In_ uint32_t const word0) noexcept;
        

        static uint8_t GetStatusBankFromFlexDataMessageFirstWord(_In_ uint32_t const word0) noexcept;
        static uint8_t GetStatusFromFlexDataMessageFirstWord(_In_ uint32_t const word0) noexcept;

        static collections::IVector<midi2::IMidiUniversalPacket> GetPacketListFromWordList(
            _In_ uint64_t const timestamp, 
            _In_ collections::IIterable<uint32_t> const& words);

        static collections::IVector<uint32_t> GetWordListFromPacketList(
            _In_ collections::IIterable<midi2::IMidiUniversalPacket> const& messages) noexcept;



    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Messages::factory_implementation
{
    struct MidiMessageHelper : MidiMessageHelperT<MidiMessageHelper, implementation::MidiMessageHelper, winrt::static_lifetime>
    {
    };
}
