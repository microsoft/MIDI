// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Legacy.MidiLegacyPortDeviceInformationAddedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    struct MidiLegacyPortDeviceInformationAddedEventArgs : MidiLegacyPortDeviceInformationAddedEventArgsT<MidiLegacyPortDeviceInformationAddedEventArgs>
    {
        MidiLegacyPortDeviceInformationAddedEventArgs() = default;

        legacy::MidiLegacyPortDeviceInformation AddedDevice();
    };
}
