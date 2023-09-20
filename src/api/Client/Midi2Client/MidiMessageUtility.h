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

        static midi2::MidiMessageType GetMessageTypeFromFirstMessageWord(_In_ uint32_t const word0) noexcept;
        static midi2::MidiPacketType GetPacketTypeFromFirstMessageWord(_In_ uint32_t const word0) noexcept;


        // This is likely to need a change in logic as new messages are added
        static bool MessageTypeHasGroupField(_In_ midi2::MidiMessageType const messageType) noexcept;
        static uint32_t ReplaceGroup(_In_ uint32_t const word0, _In_ midi2::MidiGroup const newGroup) noexcept;
        static midi2::MidiGroup GetGroup(_In_ uint32_t const word0);

        // This is likely to need a change in logic as new messages are added
        static bool MessageTypeHasChannelField(_In_ midi2::MidiMessageType const messageType) noexcept;
        static uint32_t ReplaceChannel(_In_ uint32_t const word0, _In_ midi2::MidiChannel const newChannel) noexcept;
        static midi2::MidiChannel GetChannel(_In_ uint32_t const word0);

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessageUtility : MidiMessageUtilityT<MidiMessageUtility, implementation::MidiMessageUtility, winrt::static_lifetime>
    {
    };
}
