// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiLoopbackEndpointEntry.h"
#include "MidiLoopbackEndpointEntry.g.cpp"

namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    hstring MidiLoopbackEndpointEntry::EndpointDeviceId()
    {
        throw hresult_not_implemented();
    }
    hstring MidiLoopbackEndpointEntry::Name()
    {
        throw hresult_not_implemented();
    }
    hstring MidiLoopbackEndpointEntry::Description()
    {
        throw hresult_not_implemented();
    }
}
