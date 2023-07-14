// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiUmp128.h"
#include "MidiUmp128.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    MidiUmp128::MidiUmp128(internal::MidiTimestamp timestamp, uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3)
    {
        _timestamp = timestamp;

        _ump.word0 = word0;
        _ump.word1 = word1;
        _ump.word2 = word2;
        _ump.word3 = word3;
    }

    // internal constructor for reading from the service callback
    MidiUmp128::MidiUmp128(internal::MidiTimestamp timestamp, PVOID data)
        : MidiUmp128()
    {
        WINRT_ASSERT(data != nullptr);

        _timestamp = timestamp;

        // need to have some safeties around this
        memcpy((void*)&_ump, data, sizeof(internal::PackedUmp128));
    }

}