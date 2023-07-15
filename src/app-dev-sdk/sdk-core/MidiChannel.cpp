// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiChannel.h"
#include "MidiChannel.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    uint8_t MidiChannel::Index()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiChannel::NumberForDisplay()
    {
        throw hresult_not_implemented();
    }
    bool MidiChannel::IsActive()
    {
        throw hresult_not_implemented();
    }
    hstring MidiChannel::Name()
    {
        throw hresult_not_implemented();
    }
}
