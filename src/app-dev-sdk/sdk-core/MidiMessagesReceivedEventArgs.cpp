// Copyright(c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessagesReceivedEventArgs.h"
#include "MidiMessagesReceivedEventArgs.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    hstring MidiMessagesReceivedEventArgs::SourceMidiEndpointId()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiMessageReader MidiMessagesReceivedEventArgs::GetMessageReader()
    {
        throw hresult_not_implemented();
    }
}
