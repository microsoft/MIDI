// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"

#include "MidiMessage128.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessage128 : MidiMessage128T<MidiMessage128>
    {
        MidiMessage128() = default;
        MidiMessage128(
            _In_ internal::MidiTimestamp const timestamp, 
            _In_ uint32_t const word0, 
            _In_ uint32_t const word1, 
            _In_ uint32_t const word2, 
            _In_ uint32_t const word3);

        // TODO: This doesn't do any bounds checking, and it should
        MidiMessage128(
            _In_ internal::MidiTimestamp const timestamp, 
            _In_ array_view<uint32_t const> words)
        {
            if (words.size() == 4) InternalInitializeFromPointer(timestamp, (PVOID)words.data());
        }


        // internal
        void InternalInitializeFromPointer(
            _In_ internal::MidiTimestamp const timestamp, 
            _In_ PVOID data);


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

        midi2::MidiMessageType MessageType() const noexcept 
            { return (midi2::MidiMessageType)(internal::GetUmpMessageTypeFromFirstWord(m_ump.word0)); }

        void MessageType(_In_ midi2::MidiMessageType const& value) noexcept
            { internal::SetUmpMessageType(m_ump.word0, (uint8_t)value); }

        midi2::MidiPacketType PacketType() const noexcept 
            { return midi2::MidiPacketType::UniversalMidiPacket128; }


        collections::IVector<uint32_t> GetAllWords() const noexcept;
        uint8_t AppendAllMessageWordsToList(
            _Inout_ collections::IVector<uint32_t> targetVector) const noexcept;

        uint8_t FillBuffer(
            _In_ uint32_t const byteOffset,
            _In_ foundation::IMemoryBuffer const& buffer
        ) const noexcept;

        // IStringable
        winrt::hstring ToString();

        uint32_t PeekFirstWord() { return Word0(); }

        // internal for the sending code
        internal::PackedUmp128* GetInternalUmpDataPointer() { return &m_ump; }

    private:
        internal::MidiTimestamp m_timestamp{};

        internal::PackedUmp128 m_ump{};

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessage128 : MidiMessage128T<MidiMessage128, implementation::MidiMessage128>
    {
    };
}
