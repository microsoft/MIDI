// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiMessageReceivedEventArgs.g.h"

#include "MidiUmp32.h"
#include "MidiUmp64.h"
#include "MidiUmp96.h"
#include "MidiUmp128.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessageReceivedEventArgs : MidiMessageReceivedEventArgsT<MidiMessageReceivedEventArgs>
    {
        MidiMessageReceivedEventArgs() = default;

        MidiMessageReceivedEventArgs(PVOID data, UINT sizeInBytes, LONGLONG timestamp);

        winrt::Windows::Devices::Midi2::MidiUmpPacketType UmpType();
        internal::MidiTimestamp Timestamp() { return _timestamp; }

        winrt::Windows::Devices::Midi2::IMidiUmp GetUmp();

        bool FillWords(uint32_t& word0, uint32_t& word1, uint32_t& word2, uint32_t& word3);

        bool FillUmp32(winrt::Windows::Devices::Midi2::MidiUmp32 const& ump);
        bool FillUmp64(winrt::Windows::Devices::Midi2::MidiUmp64 const& ump);
        bool FillUmp96(winrt::Windows::Devices::Midi2::MidiUmp96 const& ump);
        bool FillUmp128(winrt::Windows::Devices::Midi2::MidiUmp128 const& ump);
        
        bool FillWordArray(array_view<uint32_t> words);
        
        bool FillByteArray(array_view<uint8_t> bytes);
        
        bool FillBuffer(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t byteOffset);


    private:
        internal::MidiTimestamp _timestamp;

        // this does mean each event argument is up to 12 bytes larger than it needs to be, but this
        // is more efficient speed-wise, and avoids additional heap allocations.

        struct InternalMidiData
        {
            uint32_t Word0{ 0 };
            uint32_t Word1{ 0 };
            uint32_t Word2{ 0 };
            uint32_t Word3{ 0 };
        } _data ;

    };
}

