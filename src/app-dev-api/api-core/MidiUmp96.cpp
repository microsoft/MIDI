// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiUmp96.h"
#include "MidiUmp96.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    MidiUmp96::MidiUmp96(internal::MidiTimestamp timestamp, uint32_t word0, uint32_t word1, uint32_t word2)
    {
        _timestamp = timestamp;

        _ump.word0 = word0;
        _ump.word1 = word1;
        _ump.word2 = word2;
    }

    // internal constructor for reading from the service callback
    MidiUmp96::MidiUmp96(internal::MidiTimestamp timestamp, PVOID data)
        : MidiUmp96()
    {
      //  WINRT_ASSERT(_ump != nullptr);
        WINRT_ASSERT(data != nullptr);

        _timestamp = timestamp;

        // need to have some safeties around this
        memcpy((void*)&_ump, data, sizeof(internal::PackedUmp96));
    }


    //winrt::Windows::Foundation::IMemoryBuffer MidiUmp96::RawData()
    //{
    //    throw hresult_not_implemented();
    //}

}
