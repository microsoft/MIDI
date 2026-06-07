// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiLoopbackEndpointEntry.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackEndpointEntry : MidiLoopbackEndpointEntryT<MidiLoopbackEndpointEntry>
    {
        MidiLoopbackEndpointEntry() = default;

        hstring EndpointDeviceId();
        hstring Name();
        hstring Description();
    };
}
