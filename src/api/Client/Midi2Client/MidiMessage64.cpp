// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessage64.h"
#include "MidiMessage64.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
        MidiMessage64::MidiMessage64(
        internal::MidiTimestamp const timestamp, 
        uint32_t const word0, 
        uint32_t const word1)
    {
        m_timestamp = timestamp;

        m_ump.word0 = word0;
        m_ump.word1 = word1;
    }

    // internal constructor for reading from the service callback
    _Use_decl_annotations_
    void MidiMessage64::InternalInitializeFromPointer(
        internal::MidiTimestamp const timestamp, 
        PVOID data)
    {
        if (data == nullptr) return;

        m_timestamp = timestamp;

        // need to have some safeties around this
        memcpy((void*)&m_ump, data, sizeof(internal::PackedUmp64));
    }


}

