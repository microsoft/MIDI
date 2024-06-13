// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiServicesInitializer.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Initialization::implementation
{
    struct MidiServicesInitializer : MidiServicesInitializerT<MidiServicesInitializer>
    {
        MidiServicesInitializer() = default;

        static bool IsCompatibleDesktopAppSdkRuntimeInstalled();

        static bool IsOperatingSystemSupported();

        static bool InitializeSdkRuntime();
        static bool EnsureServiceAvailable();


        static foundation::Uri GetLatestRuntimeReleaseInstallerUri();
        static foundation::Uri GetLatestSettingsAppReleaseInstallerUri();
        static foundation::Uri GetLatestConsoleAppReleaseInstallerUri();

    private:


    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Initialization::factory_implementation
{
    struct MidiServicesInitializer : MidiServicesInitializerT<MidiServicesInitializer, implementation::MidiServicesInitializer, winrt::static_lifetime>
    {
    };
}
