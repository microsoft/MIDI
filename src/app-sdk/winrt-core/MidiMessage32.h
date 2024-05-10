// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"

#include "MidiMessage32.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiMessage32 : MidiMessage32T<MidiMessage32>
    {
        MidiMessage32() = default;
        MidiMessage32(
            _In_ internal::MidiTimestamp const timestamp, 
            _In_ uint32_t const word0);
        // internal
        void InternalInitializeFromPointer(
            _In_ internal::MidiTimestamp const timestamp, 
            _In_ PVOID data);

        uint32_t Word0() const noexcept { return m_ump.word0; }
        void Word0(_In_ uint32_t value) noexcept { m_ump.word0 = value; }

        internal::MidiTimestamp Timestamp() const noexcept { return m_timestamp; }
        void Timestamp(_In_ internal::MidiTimestamp value) noexcept { m_timestamp = value; }

        midi2::MidiMessageType MessageType() const noexcept 
            { return (midi2::MidiMessageType)(internal::GetUmpMessageTypeFromFirstWord(m_ump.word0)); }

        void MessageType(_In_ midi2::MidiMessageType const& value) noexcept 
            { internal::SetUmpMessageType(m_ump.word0, (uint8_t)value); }

        midi2::MidiPacketType PacketType() const noexcept 
            { return midi2::MidiPacketType::UniversalMidiPacket32; }

        // IStringable
        winrt::hstring ToString();

        uint32_t PeekFirstWord() { return Word0(); }

        collections::IVector<uint32_t> GetAllWords() const noexcept;
        uint8_t AppendAllMessageWordsToList(
            _Inout_ collections::IVector<uint32_t> targetVector
        ) const noexcept;

        uint8_t FillBuffer(
            _In_ uint32_t const byteOffset,
            _In_ foundation::IMemoryBuffer const& buffer
            ) const noexcept;

        // internal for the sending code
        internal::PackedUmp32* GetInternalUmpDataPointer() { return &m_ump; }

    private:
        internal::MidiTimestamp m_timestamp{};
        internal::PackedUmp32 m_ump{ };

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiMessage32 : MidiMessage32T<MidiMessage32, implementation::MidiMessage32>
    {
    };
}
