// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiServices.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiServices : MidiServicesT<MidiServices>
    {
        MidiServices() = default;

        static bool CheckForWindowsMidiServices(winrt::Microsoft::Devices::Midi2::WindowsMidiServicesCheckError& errorResult);
        static hstring GetInstalledWindowsMidiServicesVersion();
        static hstring SdkVersion();
        static hstring MinimumCompatibleMidiServicesVersion();
        static winrt::Windows::Foundation::Uri LatestMidiServicesInstallUri();
        bool IsOpen();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiServices : MidiServicesT<MidiServices, implementation::MidiServices>
    {
    };
}
