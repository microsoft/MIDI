// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "MidiUmp32.h"
#include "MidiUmp32.g.cpp"



namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    MidiUmp32::MidiUmp32(
        internal::MidiTimestamp const timestamp, 
        uint32_t const word0)
        : MidiUmp32()
    {
        m_timestamp = timestamp;
        m_ump.word0 = word0;
    }

    // internal constructor for reading from the service callback
    _Use_decl_annotations_
    MidiUmp32::MidiUmp32(
        internal::MidiTimestamp timestamp, 
        PVOID data)
        : MidiUmp32()
    {
        //WINRT_ASSERT(_ump != nullptr);
        WINRT_ASSERT(data != nullptr);

        m_timestamp = timestamp;

        // need to have some safeties around this and also make sure it gets deleted
        memcpy((void*)&m_ump, data, sizeof(internal::PackedUmp32));
    }

}
