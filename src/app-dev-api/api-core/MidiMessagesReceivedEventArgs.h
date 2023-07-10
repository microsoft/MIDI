// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiMessagesReceivedEventArgs.g.h"

#include "MidiUmp32.h";
#include "MidiUmp64.h";
#include "MidiUmp96.h";
#include "MidiUmp128.h";

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessagesReceivedEventArgs : MidiMessagesReceivedEventArgsT<MidiMessagesReceivedEventArgs>
    {
        MidiMessagesReceivedEventArgs() = default;

        // internal constructor
        MidiMessagesReceivedEventArgs(winrt::Windows::Devices::Midi2::IMidiUmp ump) { _ump = ump; }


        winrt::Windows::Devices::Midi2::IMidiUmp Ump() { return _ump; }

    private:
        winrt::Windows::Devices::Midi2::IMidiUmp _ump;

    };
}

namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessagesReceivedEventArgs : MidiMessagesReceivedEventArgsT<MidiMessagesReceivedEventArgs, implementation::MidiMessagesReceivedEventArgs>
    {
    };
}
