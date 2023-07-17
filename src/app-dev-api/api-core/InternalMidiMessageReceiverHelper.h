// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "MidiMessageReceivedEventArgs.h"
#include "MidiWordsReceivedEventArgs.h"
#include "MidiBufferReceivedEventArgs.h"

namespace Windows::Devices::Midi2::Internal
{
    class InternalMidiMessageReceiverHelper
    {
    public:
        winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs CreateMessageEventArgsFromCallbackParams(PVOID Data, UINT Size, LONGLONG Position);
        winrt::Windows::Devices::Midi2::MidiWordsReceivedEventArgs CreateWordsEventArgsFromCallbackParams(PVOID Data, UINT Size, LONGLONG Position);
        winrt::Windows::Devices::Midi2::MidiBufferReceivedEventArgs CreateBufferEventArgsFromCallbackParams(PVOID Data, UINT Size, LONGLONG Position);



    private:

    };
}
