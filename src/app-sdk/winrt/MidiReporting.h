// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Reporting.MidiReporting.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Reporting::implementation
{
    struct MidiReporting
    {
        MidiReporting() = default;

        static collections::IVector<rept::MidiServiceTransportPluginInfo> GetInstalledTransportPlugins();
        static collections::IVector<rept::MidiServiceMessageProcessingPluginInfo> GetInstalledMessageProcessingPlugins();
        static collections::IVector<rept::MidiServiceSessionInfo> GetActiveSessions();
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Reporting::factory_implementation
{
    struct MidiReporting : MidiReportingT<MidiReporting, implementation::MidiReporting>
    {
    };
}
