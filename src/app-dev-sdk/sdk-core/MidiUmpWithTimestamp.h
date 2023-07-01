// Copyright(c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiUmpWithTimestamp.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiUmpWithTimestamp : MidiUmpWithTimestampT<MidiUmpWithTimestamp>
    {
        MidiUmpWithTimestamp() = default;

        static winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp FromUmp32(winrt::Microsoft::Devices::Midi2::MidiUmp32 const& ump);
        static winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp FromUmp64(winrt::Microsoft::Devices::Midi2::MidiUmp64 const& ump);
        static winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp FromUmp96(winrt::Microsoft::Devices::Midi2::MidiUmp96 const& ump);
        static winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp FromUmp128(winrt::Microsoft::Devices::Midi2::MidiUmp128 const& ump);
        uint64_t Timestamp();
        void Timestamp(uint64_t value);
        winrt::Windows::Foundation::Collections::IVector<uint32_t> Words();
        winrt::Microsoft::Devices::Midi2::MidiUmpMessageType MessageType();
        hstring ToString();
    private:
        winrt::Windows::Foundation::Collections::IVector<uint32_t>
            _words{ winrt::single_threaded_vector<uint32_t>() };

        uint64_t _timestamp;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiUmpWithTimestamp : MidiUmpWithTimestampT<MidiUmpWithTimestamp, implementation::MidiUmpWithTimestamp>
    {
    };
}
