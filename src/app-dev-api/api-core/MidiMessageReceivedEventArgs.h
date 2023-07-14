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

        // internal constructor
        //MidiMessagesReceivedEventArgs(winrt::Windows::Devices::Midi2::IMidiUmp ump) { _ump = ump; }

        // internal
        void Ump(winrt::Windows::Devices::Midi2::IMidiUmp value) { _ump = value; }
        winrt::Windows::Devices::Midi2::IMidiUmp Ump() { return _ump; }


        //com_array<uint32_t> Words() { return com_array<uint32_t>(_words); }
        //uint32_t WordCount() { return _wordCount; }
        //uint64_t Timestamp() { return _timestamp; }

        //void InternalSetData(uint32_t* data, uint32_t count, uint64_t timestamp) { _wordCount = count; _timestamp = timestamp; memcpy((void*)&_words, data, count * sizeof(uint32_t)); }

    private:
        // hack
        //std::array<uint32_t, 4> _words;
        //uint32_t _wordCount;
        //uint64_t _timestamp;

        winrt::Windows::Devices::Midi2::IMidiUmp _ump;

    };
}

namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessageReceivedEventArgs : MidiMessageReceivedEventArgsT<MidiMessageReceivedEventArgs, implementation::MidiMessageReceivedEventArgs>
    {
    };
}
