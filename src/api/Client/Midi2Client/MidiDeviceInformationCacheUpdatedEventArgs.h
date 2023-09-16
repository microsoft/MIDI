// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiDeviceInformationCacheUpdatedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiDeviceInformationCacheUpdatedEventArgs : MidiDeviceInformationCacheUpdatedEventArgsT<MidiDeviceInformationCacheUpdatedEventArgs>
    {
        MidiDeviceInformationCacheUpdatedEventArgs() = default;

        winrt::Windows::Devices::Midi2::MidiCacheUpdateType UpdateType();
        hstring DeviceId();
        hstring Key();
    };
}
