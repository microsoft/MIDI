// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Legacy.MidiLegacyPortDeviceInformationRemovedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    struct MidiLegacyPortDeviceInformationRemovedEventArgs : MidiLegacyPortDeviceInformationRemovedEventArgsT<MidiLegacyPortDeviceInformationRemovedEventArgs>
    {
        MidiLegacyPortDeviceInformationRemovedEventArgs() = default;

        winrt::hstring PortDeviceId();
        enumeration::DeviceInformationUpdate DeviceInformationUpdate();
    };
}
