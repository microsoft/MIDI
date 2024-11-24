// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
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

        static bool InitializeDesktopAppSdkRuntime();
        static bool ShutdownDesktopAppSdkRuntime();
        static bool IsCompatibleDesktopAppSdkRuntimeInstalled();

        static bool IsOperatingSystemSupported();
        static bool EnsureServiceAvailable();


        static foundation::Uri GetLatestDesktopAppSdkRuntimeReleaseInstallerUri();
        //static foundation::Uri GetLatestSettingsAppReleaseInstallerUri();
        //static foundation::Uri GetLatestConsoleAppReleaseInstallerUri();

    private:
        static std::wstring m_sdkLocation;
        static HMODULE m_moduleHandle;

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Initialization::factory_implementation
{
    struct MidiServicesInitializer : MidiServicesInitializerT<MidiServicesInitializer, implementation::MidiServicesInitializer, winrt::static_lifetime>
    {
    };
}
