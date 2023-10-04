// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointDeviceWatcher.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointDeviceWatcher : MidiEndpointDeviceWatcherT<MidiEndpointDeviceWatcher>
    {
        MidiEndpointDeviceWatcher() = default;

        void Start();
        void Stop();

        collections::IMap<winrt::hstring, midi2::MidiEndpointDeviceInformation> EndpointDevices();
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointDeviceWatcher : MidiEndpointDeviceWatcherT<MidiEndpointDeviceWatcher, implementation::MidiEndpointDeviceWatcher>
    {
    };
}
