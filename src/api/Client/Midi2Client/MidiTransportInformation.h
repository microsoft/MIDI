// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiTransportInformation.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiTransportInformation : MidiTransportInformationT<MidiTransportInformation>
    {
        MidiTransportInformation() = default;

        hstring Id();
        hstring Name();
        hstring ShortName();
        hstring IconPath();
        hstring Author();
        hstring ServicePluginFileName();
        bool IsRuntimeCreatable();
    };
}
