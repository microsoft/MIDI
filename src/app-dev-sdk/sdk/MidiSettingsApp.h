// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiSettingsApp.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiSettingsApp : MidiSettingsAppT<MidiSettingsApp>
    {
        MidiSettingsApp() = default;

        static bool IsSettingsAppInstalled();
        static winrt::Windows::Foundation::Uri SettingsAppInstallUri();
        static void ShowEndpointPropertiesWindow(hstring const& endpointId, winrt::Microsoft::Devices::Midi2::MidiSettingsAppDisplayMode const& displayMode);
        static hstring ShowEndpointCreationWindow(hstring const& transportId, winrt::Microsoft::Devices::Midi2::MidiSettingsAppDisplayMode const& displayMode);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiSettingsApp : MidiSettingsAppT<MidiSettingsApp, implementation::MidiSettingsApp>
    {
    };
}
