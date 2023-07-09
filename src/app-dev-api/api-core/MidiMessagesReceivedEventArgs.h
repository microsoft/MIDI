// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiMessagesReceivedEventArgs.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessagesReceivedEventArgs : MidiMessagesReceivedEventArgsT<MidiMessagesReceivedEventArgs>
    {
        MidiMessagesReceivedEventArgs() = default;

        winrt::Windows::Devices::Midi2::IMidiInputConnection Source() { throw hresult_not_implemented(); }

    private:
        com_ptr<IMidiInputConnection> _source{ nullptr };

    };
}
