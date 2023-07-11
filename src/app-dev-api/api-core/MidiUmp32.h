// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"

#include "MidiUmp32.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiUmp32 : MidiUmp32T<MidiUmp32>
    {
        MidiUmp32();
        MidiUmp32(uint64_t timestamp, uint32_t word0);

        // internal
        void SetData(PVOID data);

        uint32_t Word0() { return _ump->word0; }
        void Word0(uint32_t value) { _ump->word0 = value; }

        uint64_t Timestamp() { return _ump->timestamp; }
        void Timestamp(uint64_t value) { _ump->timestamp = value; }


        winrt::Windows::Devices::Midi2::MidiUmpMessageType MessageType() { return (winrt::Windows::Devices::Midi2::MidiUmpMessageType)(internal::GetUmpMessageTypeFromFirstWord(_ump->word0)); }
        winrt::Windows::Devices::Midi2::MidiUmpPacketType MidiUmpPacketType() { return winrt::Windows::Devices::Midi2::MidiUmpPacketType::Ump32; }

        winrt::Windows::Foundation::IMemoryBuffer RawData();

    private:
        Windows::Foundation::MemoryBuffer _umpBackingStore = Windows::Foundation::MemoryBuffer(sizeof(intshared::PackedUmp32));

        intshared::PackedUmp32* _ump{ nullptr };

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiUmp32 : MidiUmp32T<MidiUmp32, implementation::MidiUmp32>
    {
    };
}
