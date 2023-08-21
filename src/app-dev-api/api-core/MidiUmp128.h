// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"

#include "MidiUmp128.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiUmp128 : MidiUmp128T<MidiUmp128>
    {
        MidiUmp128() = default;
        MidiUmp128(
            _In_ internal::MidiTimestamp const timestamp, 
            _In_ uint32_t const word0, 
            _In_ uint32_t const word1, 
            _In_ uint32_t const word2, 
            _In_ uint32_t const word3);

        // TODO: This doesn't do any bounds checking, and it should
        MidiUmp128(
            _In_ internal::MidiTimestamp const timestamp, 
            _In_ array_view<uint32_t const> words) : MidiUmp128(timestamp, (PVOID)words.data()) {};

        // internal
        MidiUmp128(_In_ internal::MidiTimestamp const timestamp, _In_ PVOID data);


        uint32_t Word0() const noexcept { return m_ump.word0; }
        void Word0(_In_ uint32_t value) noexcept { m_ump.word0 = value; }

        uint32_t Word1() const noexcept { return m_ump.word1; }
        void Word1(_In_ uint32_t value) noexcept { m_ump.word1 = value; }

        uint32_t Word2() const noexcept { return m_ump.word2; }
        void Word2(_In_ uint32_t value) noexcept { m_ump.word2 = value; }
        
        uint32_t Word3() const noexcept { return m_ump.word3; }
        void Word3(_In_ uint32_t value) noexcept { m_ump.word3 = value; }

        internal::MidiTimestamp Timestamp() const noexcept { return m_timestamp; }
        void Timestamp(_In_ internal::MidiTimestamp value) noexcept { m_timestamp = value; }

        winrt::Windows::Devices::Midi2::MidiUmpMessageType MessageType() const noexcept { return (winrt::Windows::Devices::Midi2::MidiUmpMessageType)(internal::GetUmpMessageTypeFromFirstWord(m_ump.word0)); }
        void MessageType(_In_ winrt::Windows::Devices::Midi2::MidiUmpMessageType const& value) noexcept { internal::SetUmpMessageType(m_ump.word0, (uint8_t)value); }

        winrt::Windows::Devices::Midi2::MidiUmpPacketType UmpPacketType() const noexcept { return winrt::Windows::Devices::Midi2::MidiUmpPacketType::Ump128; }

        // internal for the sending code
        internal::PackedUmp128* GetInternalUmpDataPointer() { return &m_ump; }

    private:
        internal::MidiTimestamp m_timestamp{};

        internal::PackedUmp128 m_ump{};

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiUmp128 : MidiUmp128T<MidiUmp128, implementation::MidiUmp128>
    {
    };
}
