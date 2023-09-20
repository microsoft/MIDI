// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "MidiMessage32.h"
#include "MidiMessage32.g.cpp"



namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    MidiMessage32::MidiMessage32(
        internal::MidiTimestamp const timestamp, 
        uint32_t const word0)
    {
        m_timestamp = timestamp;
        m_ump.word0 = word0;
    }

    // internal constructor for reading from the service callback
    // we needed a second function here because overload with the other
    // constructor caused access violations when wrong one was picked
    _Use_decl_annotations_
    void MidiMessage32::InternalInitializeFromPointer(
        internal::MidiTimestamp timestamp, 
        PVOID data)
    {
        if (data == nullptr) return;

        m_timestamp = timestamp;

        memcpy((void*)&m_ump, data, sizeof(internal::PackedUmp32));
    }

}
