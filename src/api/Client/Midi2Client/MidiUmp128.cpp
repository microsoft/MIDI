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
    MidiUmp128::MidiUmp128(
        _In_ internal::MidiTimestamp const timestamp, 
        _In_ uint32_t const word0, 
        _In_ uint32_t const word1, 
        _In_ uint32_t const word2, 
        _In_ uint32_t const word3)
    {
        m_timestamp = timestamp;

        m_ump.word0 = word0;
        m_ump.word1 = word1;
        m_ump.word2 = word2;
        m_ump.word3 = word3;
    }

    // internal constructor for reading from the service callback
    MidiUmp128::MidiUmp128(_In_ internal::MidiTimestamp timestamp, _In_ PVOID data)
        : MidiUmp128()
    {
        WINRT_ASSERT(data != nullptr);

        m_timestamp = timestamp;

        // need to have some safeties around this
        memcpy((void*)&m_ump, data, sizeof(internal::PackedUmp128));
    }

}