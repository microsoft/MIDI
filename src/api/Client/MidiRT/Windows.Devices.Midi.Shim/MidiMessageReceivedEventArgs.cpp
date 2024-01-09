// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageReceivedEventArgs.h"
#include "MidiMessageReceivedEventArgs.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    midi1::IMidiMessage MidiMessageReceivedEventArgs::Message()
    {
        throw hresult_not_implemented();
    }
}
