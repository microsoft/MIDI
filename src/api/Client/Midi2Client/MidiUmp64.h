// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "pch.h"

#include "MidiUmp64.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiUmp64 : MidiUmp64T<MidiUmp64>
    {
        MidiUmp64() = default;
        MidiUmp64(
            _In_ internal::MidiTimestamp const timestamp, 
            _In_ uint32_t const word0, 
            _In_ uint32_t const word1);

        // TODO: This doesn't do any bounds checking, and it should
        MidiUmp64(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ array_view<uint32_t const> words)
        {
            if (words.size() == 2) InternalInitializeFromPointer(timestamp, (PVOID)words.data());
        }

        // internal
        void InternalInitializeFromPointer(
            _In_ internal::MidiTimestamp timestamp, 
            _In_ PVOID data);

        uint32_t Word0() const noexcept { return m_ump.word0; }
        void Word0(_In_ uint32_t value) noexcept { m_ump.word0 = value; }

        uint32_t Word1() const noexcept { return m_ump.word1; }
        void Word1(_In_ uint32_t value) noexcept { m_ump.word1 = value; }

        internal::MidiTimestamp Timestamp() const noexcept { return m_timestamp; }
        void Timestamp(_In_ internal::MidiTimestamp value) noexcept { m_timestamp = value; }

        winrt::Windows::Devices::Midi2::MidiUmpMessageType MessageType() const noexcept 
            { return (winrt::Windows::Devices::Midi2::MidiUmpMessageType)(internal::GetUmpMessageTypeFromFirstWord(m_ump.word0)); }

        void MessageType(_In_ winrt::Windows::Devices::Midi2::MidiUmpMessageType const& value) noexcept 
            { internal::SetUmpMessageType(m_ump.word0, (uint8_t)value); }

        winrt::Windows::Devices::Midi2::MidiUmpPacketType UmpPacketType() const noexcept 
            { return winrt::Windows::Devices::Midi2::MidiUmpPacketType::Ump64; }

        // internal for the sending code
        internal::PackedUmp64* GetInternalUmpDataPointer() { return &m_ump; }

    private:
        internal::MidiTimestamp m_timestamp{};

        internal::PackedUmp64 m_ump{};

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiUmp64 : MidiUmp64T<MidiUmp64, implementation::MidiUmp64>
    {
    };
}
