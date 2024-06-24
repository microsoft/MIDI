// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiReporting.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Diagnostics::implementation
{
    struct MidiReporting
    {
        MidiReporting() = default;

        static collections::IVector<diag::MidiServiceTransportPluginInfo> GetInstalledTransportPlugins();
        static collections::IVector<diag::MidiServiceMessageProcessingPluginInfo> GetInstalledMessageProcessingPlugins();
        static collections::IVector<diag::MidiServiceSessionInfo> GetActiveSessions();
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Diagnostics::factory_implementation
{
    struct MidiReporting : MidiReportingT<MidiReporting, implementation::MidiReporting>
    {
    };
}
