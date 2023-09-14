// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiUmpUtility.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiUmpUtility
    {
        MidiUmpUtility() = default;

        static bool ValidateUmp32MessageType(_In_ uint32_t const word0) noexcept;
        static bool ValidateUmp64MessageType(_In_ uint32_t const word0) noexcept;
        static bool ValidateUmp96MessageType(_In_ uint32_t const word0) noexcept;
        static bool ValidateUmp128MessageType(_In_ uint32_t const word0) noexcept;

        static midi2::MidiUmpMessageType GetMessageTypeFromFirstUmpWord(_In_ uint32_t const word0) noexcept;
        static midi2::MidiUmpPacketType GetPacketTypeFromFirstUmpWord(_In_ uint32_t const word0) noexcept;

        static uint32_t ReplaceGroup(_In_ uint32_t const word0, _In_ midi2::MidiGroup const newGroup) noexcept;

        // This is likely to need a change in logic as new messages are added
        static bool MessageTypeHasGroupField(_In_ midi2::MidiUmpMessageType const messageType) noexcept;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiUmpUtility : MidiUmpUtilityT<MidiUmpUtility, implementation::MidiUmpUtility>
    {
    };
}
