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
        MidiUmp128(uint64_t timestamp, uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3);

        // internal
        MidiUmp128(PVOID data);


        uint32_t Word0() { return _ump->word0; }
        void Word0(uint32_t value) { _ump->word0 = value; }
        
        uint32_t Word1() { return _ump->word1; }
        void Word1(uint32_t value) { _ump->word1 = value; }
        
        uint32_t Word2() { return _ump->word2; }
        void Word2(uint32_t value) { _ump->word2 = value; }
        
        uint32_t Word3() { return _ump->word3; }
        void Word3(uint32_t value) { _ump->word3 = value; }

        uint64_t Timestamp() { return _ump->timestamp; }
        void Timestamp(uint64_t value) { _ump->timestamp = value; }

        winrt::Windows::Devices::Midi2::MidiUmpMessageType MessageType() { return (winrt::Windows::Devices::Midi2::MidiUmpMessageType)(internal::GetUmpMessageTypeFromFirstWord(_ump->word0)); }
        void MessageType(winrt::Windows::Devices::Midi2::MidiUmpMessageType const& value) { internal::SetUmpMessageType(_ump->word0, (uint8_t)value); }

        winrt::Windows::Devices::Midi2::MidiUmpPacketType MidiUmpPacketType() { return winrt::Windows::Devices::Midi2::MidiUmpPacketType::Ump128; }

        winrt::Windows::Foundation::IMemoryBuffer RawData();

    private:
        intshared::PackedUmp128* _ump{};

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiUmp128 : MidiUmp128T<MidiUmp128, implementation::MidiUmp128>
    {
    };
}
